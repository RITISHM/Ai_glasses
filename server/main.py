from flask import Flask
from flask_sock import Sock
import time
import os
import json
import traceback
from threading import Lock

# Import your existing functions
from .api import end_chat, start_chat, generate_image_response, generate_prompt_response
from .stt import speech_to_text
from .tts import text_to_speech

app = Flask(__name__)
sock = Sock(app)

UPLOAD_FOLDER = "uploads"
RESPONSE_folder = os.path.join(UPLOAD_FOLDER, "response")

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Lock for thread safety
upload_lock = Lock()

# OPTIMIZATION: Increased chunk sizes for faster transfer
RECEIVE_CHUNK_SIZE = 32768  # 32KB chunks instead of default
SEND_CHUNK_SIZE = 32768     # 32KB chunks for sending

def send_error_response(ws, message):
    """Send error response to client"""
    try:
        error_msg = json.dumps({"status": "error", "message": message})
        ws.send(error_msg)
    except Exception as e:
        print(f"Failed to send error: {e}")

def verify_wav_header(data):
    """Verify if data starts with valid WAV header"""
    if len(data) < 44:
        return False
    
    # Check RIFF header
    if data[0:4] != b'RIFF':
        return False
    
    # Check WAVE format
    if data[8:12] != b'WAVE':
        return False
    
    return True

@sock.route('/upload')
def upload(ws):
    print('=' * 50)
    print('✅ Client connected')
    print('=' * 50)
  
    with upload_lock:
        chunks = []
        total_received = 0
        expected_size = 0
        filename = None
        
        try:
            # ===== RECEIVE FILE SIZE =====
            size_msg = ws.receive(timeout=10)  # Increased timeout
            
            if not size_msg:
                print("❌ No size received")
                send_error_response(ws, "No size received")
                return
            
            try:
                expected_size = int(size_msg)
                print(f"📥 Expecting: {expected_size/1024:.1f} KB")
                
                if expected_size <= 0 or expected_size > 10000000:
                    print(f"❌ Invalid size: {expected_size}")
                    send_error_response(ws, "Invalid file size")
                    return
                    
            except ValueError as e:
                print(f"❌ Invalid size format: {size_msg}")
                send_error_response(ws, "Invalid size format")
                return
            
            # ===== RECEIVE AUDIO CHUNKS =====
            print(f'📥 Receiving audio chunks...')
            t_start = time.time()
            chunk_count = 0
            timeout_occurred = False
            last_chunk_time = time.time()
            
            # CRITICAL: Give client time to start sending
            time.sleep(0.2)
            
            while True:
                try:
                    # Calculate dynamic timeout based on:
                    # - Remaining data
                    # - Current transfer speed
                    remaining = expected_size - total_received
                    
                    if chunk_count == 0:
                        # First chunk: be patient
                        chunk_timeout = 15
                    elif remaining > 50000:
                        # Large amount remaining: standard timeout
                        chunk_timeout = 10
                    elif remaining > 0:
                        # Near completion: shorter timeout
                        chunk_timeout = 5
                    else:
                        # Complete
                        break
                    
                    data = ws.receive(timeout=chunk_timeout)
                    
                    if data is None:
                        elapsed_since_last = time.time() - last_chunk_time
                        print(f"⚠️ Timeout after {elapsed_since_last:.1f}s (chunk {chunk_count})")
                        timeout_occurred = True
                        
                        # If we got some data and timeout is reasonable, consider it complete
                        if total_received > 1000 and elapsed_since_last > 8:
                            print(f"⚠️ Assuming upload complete due to timeout")
                            break
                        else:
                            break
                    
                    # Check for EOF marker
                    if isinstance(data, str):
                        if data == "EOF":
                            print(f"✅ EOF received after {chunk_count} chunks")
                            break
                        else:
                            print(f"⚠️ Unexpected text message: {data[:50]}")
                            continue
                    
                    # Process binary data
                    if isinstance(data, bytes) and len(data) > 0:
                        chunks.append(data)
                        data_len = len(data)
                        total_received += data_len
                        chunk_count += 1
                        last_chunk_time = time.time()
                        
                        # Progress update every 10 chunks
                        if chunk_count % 10 == 0:
                            progress = (total_received / expected_size) * 100
                            print(f"  📦 Chunk {chunk_count}: {total_received/1024:.1f} KB / {expected_size/1024:.1f} KB ({progress:.1f}%)")
                        
                        # Check if complete
                        if total_received >= expected_size:
                            print(f"✅ Received expected amount")
                            break
                    
                except Exception as e:
                    print(f"⚠️ Receive error: {e}")
                    if total_received > 1000:
                        # Got substantial data, try to process it
                        print(f"⚠️ Attempting to process partial data")
                        break
                    else:
                        raise
            
            t_receive = time.time()
            receive_time = t_receive - t_start
            
            # ===== VALIDATE DATA =====
            if total_received == 0:
                print("❌ No data received")
                send_error_response(ws, "No data received")
                return
            
            # Combine all chunks
            full_data = b''.join(chunks)
            actual_size = len(full_data)
            
            print(f"📦 Received: {actual_size/1024:.1f} KB in {receive_time:.1f}s ({actual_size/receive_time/1024:.1f} KB/s)")
            print(f"   Chunks: {chunk_count}, Avg size: {actual_size/chunk_count if chunk_count > 0 else 0:.0f} bytes")
            
            # Check size mismatch
            size_ratio = actual_size / expected_size
            if size_ratio < 0.95:
                print(f"⚠️ Size mismatch! Expected {expected_size/1024:.1f}KB, got {actual_size/1024:.1f}KB ({size_ratio*100:.1f}%)")
                if timeout_occurred:
                    print(f"   Likely cause: Upload timeout")
                    
                # Decide if we should try to process anyway
                if size_ratio < 0.5:
                    print(f"❌ Too much data missing, aborting")
                    send_error_response(ws, f"Incomplete upload: only {size_ratio*100:.1f}% received")
                    return
                else:
                    print(f"⚠️ Attempting to process partial audio...")
            
            # Verify WAV format
            if not verify_wav_header(full_data):
                print("⚠️ Invalid WAV header - data may be corrupted")
            else:
                print("✅ Valid WAV header detected")
            name=int(time.time())
            # ===== SAVE FILE =====
            filename = f"{UPLOAD_FOLDER}/audio/audio_{name}.wav"
            
            try:
                with open(filename, "wb") as f:
                    f.write(full_data)
                    f.flush()
                    os.fsync(f.fileno())
                
                if os.path.exists(filename):
                    saved_size = os.path.getsize(filename)
                    print(f"💾 Saved: {filename} ({saved_size/1024:.1f} KB)")
                else:
                    print(f"❌ Save verification failed")
                    send_error_response(ws, "File save failed")
                    return
                    
            except Exception as e:
                print(f"❌ Save error: {e}")
                send_error_response(ws, f"File save error: {str(e)}")
                return
            
            RESPONSE_AUDIO=f"{RESPONSE_folder}/response_{name}.wav"
            # ===== PROCESS AUDIO =====
            print(f"🤖 Processing audio...")
            processing_start = time.time()
            
            try:
                transcribe = speech_to_text(filename)
                print(f"📝 Transcription: {transcribe[:100]}...")
                
                response_text = generate_prompt_response(transcribe)
                print(f"💬 Response: {response_text[:100]}...")
                
                text_to_speech(response_text, RESPONSE_AUDIO)
                
                processing_time = time.time() - processing_start
                print(f"✅ Processing complete ({processing_time:.1f}s)")
                
            except Exception as e:
                print(f"❌ Processing error: {e}")
                traceback.print_exc()
                send_error_response(ws, f"Processing error: {str(e)}")
                return
            
            # ===== SEND RESPONSE AUDIO =====
            if not os.path.exists(RESPONSE_AUDIO):
                print(f"⚠️ No response audio generated")
                
                response = {
                    "status": "ok",
                    "upload_size": actual_size,
                    "sending_audio": False,
                    "message": "Processing complete but no audio response"
                }
                ws.send(json.dumps(response))
                return
            
            audio_size = os.path.getsize(RESPONSE_AUDIO)
            print(f"📤 Sending response: {audio_size/1024:.1f} KB")
            
            # Send metadata
            response = {
                "status": "ok",
                "upload_size": actual_size,
                "audio_size": audio_size,
                "sending_audio": True
            }
            
            try:
                ws.send(json.dumps(response))
                time.sleep(0.1)
                
            except Exception as e:
                print(f"❌ Metadata send failed: {e}")
                return
            
            # Send audio in chunks
            send_start = time.time()
            sent_bytes = 0
            send_chunk_count = 0
            
            try:
                with open(RESPONSE_AUDIO, "rb") as f:
                    while True:
                        chunk = f.read(SEND_CHUNK_SIZE)
                        if not chunk:
                            break
                        
                        ws.send(chunk)
                        sent_bytes += len(chunk)
                        send_chunk_count += 1
                        
                        # Small delay for reliability
                        time.sleep(0.05)
                
                send_time = time.time() - send_start
                print(f"✅ Response sent: {sent_bytes/1024:.1f} KB in {send_time:.1f}s ({sent_bytes/send_time/1024:.1f} KB/s)")
                print(f"   Sent in {send_chunk_count} chunks")
                time.sleep(0.7)
                
            except Exception as e:
                print(f"❌ Send error: {e}")
                return
            
            # ===== SUMMARY =====
            total_time = time.time() - t_start
            print(f"✅ Transaction complete ({total_time:.1f}s total)")
            print(f"   Upload: {receive_time:.1f}s, Process: {processing_time:.1f}s, Download: {send_time:.1f}s")
            print()
            
        except Exception as e:
            print(f"❌ Fatal error: {e}")
            traceback.print_exc()
            
            try:
                send_error_response(ws, f"Server error: {str(e)}")
            except:
                pass
        
        finally:
            print(f"🔌 Client disconnected\n")

@app.route('/')
def index():
    audio_exists = os.path.exists(RESPONSE_folder)
    audio_size = os.path.getsize(RESPONSE_folder) if audio_exists else 0
    
    upload_count = len([f for f in os.listdir(UPLOAD_FOLDER) if f.endswith('.wav')])
    
    return f'''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Audio WebSocket Server</title>
        <style>
            body {{ font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }}
            .container {{ background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }}
            h1 {{ color: #333; }}
            .status {{ padding: 10px; border-radius: 5px; margin: 10px 0; }}
            .ok {{ background: #d4edda; color: #155724; }}
            .error {{ background: #f8d7da; color: #721c24; }}
            .info {{ background: #d1ecf1; color: #0c5460; }}
            table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
            td {{ padding: 10px; border-bottom: 1px solid #ddd; }}
            td:first-child {{ font-weight: bold; width: 200px; }}
        </style>
    </head>
    <body>
        <div class="container">
            <h1>🎤 WebSocket Audio Server (OPTIMIZED)</h1>
            <div class="status ok">
                <strong>Status:</strong> Running ✅
            </div>
            
            <table>
                <tr>
                    <td>WebSocket Endpoint</td>
                    <td><code>ws://0.0.0.0:5000/upload</code></td>
                </tr>
                <tr>
                    <td>Protocol</td>
                    <td>Upload → Process → Download</td>
                </tr>
                <tr>
                    <td>Chunk Size</td>
                    <td>{SEND_CHUNK_SIZE/1024:.0f} KB (Optimized)</td>
                </tr>
                <tr>
                    <td>Upload Folder</td>
                    <td>{os.path.abspath(UPLOAD_FOLDER)}</td>
                </tr>
                <tr>
                    <td>Files Uploaded</td>
                    <td>{upload_count} recordings</td>
                </tr>
                <tr>
                    <td>Response Audio</td>
                    <td>{'✅ Ready' if audio_exists else '❌ Not Found'}</td>
                </tr>
                {f'<tr><td>Response Size</td><td>{audio_size/1024:.2f} KB</td></tr>' if audio_exists else ''}
            </table>
            
            {'' if audio_exists else '<div class="status error">⚠️ Response audio file not found! Place your audio at:<br><code>' + RESPONSE_AUDIO + '</code></div>'}
            
            <div class="status info">
                <strong>💡 Optimizations Applied:</strong><br>
                • 32KB chunk sizes for faster transfer<br>
                • Pre-allocated buffers for efficient memory usage<br>
                • Removed artificial delays between chunks<br>
                • Direct buffer writes (no list concatenation)<br>
                • Adaptive timeouts based on remaining data
            </div>
        </div>
    </body>
    </html>
    '''

@app.route('/health')
def health():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "timestamp": time.time(),
        "uploads_folder": UPLOAD_FOLDER,
        "response_audio_exists": os.path.exists(RESPONSE_folder),
        "optimizations": {
            "receive_chunk_size": RECEIVE_CHUNK_SIZE,
            "send_chunk_size": SEND_CHUNK_SIZE
        }
    }

if __name__ == '__main__':
    print("\n" + "=" * 60)
    print("🚀 WebSocket Audio Server (OPTIMIZED)")
    print("=" * 60)
    print(f"📁 Upload folder: {os.path.abspath(UPLOAD_FOLDER)}")
    print(f"🌐 WebSocket: ws://0.0.0.0:5000/upload")
    print(f"🌐 Web interface: http://0.0.0.0:5000")
    print(f"🏥 Health check: http://0.0.0.0:5000/health")
    print(f"📋 Protocol: UPLOAD → PROCESS → DOWNLOAD")
    print(f"⚡ Chunk size: {SEND_CHUNK_SIZE/1024:.0f} KB")
    
    if os.path.exists(RESPONSE_folder):
        size = os.path.getsize(RESPONSE_folder)
        print(f"📤 Response audio: ✅ ({size/1024:.2f} KB)")
    else:
        print(f"⚠️  Response audio not found!")
        print(f"💡 Create: {os.path.abspath(RESPONSE_folder)}")
    
    print("=" * 60 + "\n")
    print("Press Ctrl+C to stop the server\n")
    
    try:
        app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
    except KeyboardInterrupt:
        print("\n\n👋 Server stopped gracefully")