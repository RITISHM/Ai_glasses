from flask import Flask
from flask_sock import Sock
import time
import os
import json

app = Flask(__name__)
sock = Sock(app)

UPLOAD_FOLDER = "uploads"
RESPONSE_AUDIO = r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\file_example_WAV_2MG.wav"  # Path to audio file to send back

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@sock.route('/upload')
def upload(ws):
    print('✅ Client connected')
    
    try:
        # ===== STEP 1: RECEIVE UPLOAD =====
        # First message should be file size
        size_msg = ws.receive()
        if not size_msg:
            print("❌ No size received")
            ws.send('{"status":"error","message":"No size"}')
            return
            
        try:
            expected_size = int(size_msg)
            print(f"📏 Expected size: {expected_size} bytes ({expected_size/1024:.2f} KB)")
        except:
            print(f"⚠️ Invalid size message: {size_msg}")
            expected_size = 0
        
        # Receive binary chunks
        chunks = []
        total_received = 0
        t0 = time.time()
        chunk_count = 0
        
        while True:
            try:
                data = ws.receive(timeout=3)
                
                if data is None:
                    break
                    
                # Check for EOF marker
                if isinstance(data, str) and data == "EOF":
                    print(f"📨 Received EOF marker")
                    break
                
                if isinstance(data, bytes):
                    chunks.append(data)
                    total_received += len(data)
                    chunk_count += 1
                    
                    # Progress update every 20 chunks
                    if chunk_count % 20 == 0:
                        progress = (total_received / expected_size * 100) if expected_size > 0 else 0
                        print(f"  📦 Chunk {chunk_count}: {total_received/1024:.1f} KB ({progress:.1f}%)")
                
            except Exception as e:
                print(f"⚠️ Receive timeout or error: {e}")
                break
        
        t_receive = time.time()
        receive_time = t_receive - t0
        
        if not chunks:
            print("⚠️ No data received")
            ws.send('{"status":"error","message":"No data"}')
            return
        
        # Combine all chunks
        full_data = b''.join(chunks)
        
        print(f"\n📦 Total received: {len(full_data)} bytes in {chunk_count} chunks")
        
        filename = f"{UPLOAD_FOLDER}/audio_{int(time.time())}.wav"
        
        # Write to file
        t_write_start = time.time()
        with open(filename, "wb") as f:
            f.write(full_data)
        t_write_end = time.time()
        
        write_time = t_write_end - t_write_start
        
        size_kb = len(full_data) / 1024
        speed = size_kb / receive_time if receive_time > 0 else 0
        
        print(f"⏱️  Receive time: {receive_time:.3f}s")
        print(f"💾 Write time: {write_time:.3f}s")
        print(f"🚀 Upload speed: {speed:.2f} KB/s")
        print(f"✅ Saved: {filename}\n")
        
        # ===== STEP 2: SEND AUDIO BACK =====
        if os.path.exists(RESPONSE_AUDIO):
            print(f"📤 Sending response audio: {RESPONSE_AUDIO}")
            
            # Get file size
            audio_size = os.path.getsize(RESPONSE_AUDIO)
            print(f"📊 Response audio size: {audio_size} bytes ({audio_size/1024:.2f} KB)")
            
            # Send metadata first
            response = {
                "status": "ok",
                "upload_size": len(full_data),
                "upload_chunks": chunk_count,
                "upload_time": receive_time,
                "upload_speed": speed,
                "audio_size": audio_size,
                "sending_audio": True
            }
            ws.send(json.dumps(response))
            print("📨 Sent metadata")
            
            time.sleep(0.1)  # Brief delay
            
            # Send audio file in chunks
            CHUNK_SIZE = 4096
            sent_bytes = 0
            chunk_num = 0
            
            t_send_start = time.time()
            with open(RESPONSE_AUDIO, "rb") as f:
                while True:
                    chunk = f.read(CHUNK_SIZE)
                    if not chunk:
                        break
                    
                    try:
                        ws.send(chunk)
                        sent_bytes += len(chunk)
                        chunk_num += 1
                        
                        # Progress every 20 chunks
                        if chunk_num % 20 == 0:
                            progress = (sent_bytes / audio_size * 100)
                            print(f"  📤 Sent: {sent_bytes/1024:.1f} KB ({progress:.1f}%)")
                        
                        time.sleep(0.001)  # Small delay for stability
                    except Exception as e:
                        print(f"❌ Send error: {e}")
                        break
            
            # Wait a moment before closing to ensure all data is received
            time.sleep(0.5)
            
            t_send_end = time.time()
            send_time = t_send_end - t_send_start
            send_speed = (audio_size / 1024.0) / send_time if send_time > 0 else 0
            
            print(f"✅ Sent {sent_bytes} bytes in {chunk_num} chunks")
            print(f"⏱️  Send time: {send_time:.3f}s")
            print(f"🚀 Download speed: {send_speed:.2f} KB/s\n")
            
        else:
            print(f"⚠️ Response audio not found: {RESPONSE_AUDIO}")
            print("💡 Create a file named 'response_audio.wav' in the same folder as this script")
            
            # Send response without audio
            response = {
                "status": "ok",
                "upload_size": len(full_data),
                "upload_chunks": chunk_count,
                "upload_time": receive_time,
                "upload_speed": speed,
                "sending_audio": False,
                "message": "No response audio file found"
            }
            ws.send(json.dumps(response))
        
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        try:
            ws.send(f'{{"status":"error","message":"{str(e)}"}}')
        except:
            pass
    
    finally:
        print('🔌 Disconnected\n')

@app.route('/')
def index():
    audio_exists = os.path.exists(RESPONSE_AUDIO)
    audio_size = os.path.getsize(RESPONSE_AUDIO) if audio_exists else 0
    
    return f'''
    <h1>WebSocket Audio Server (Bidirectional)</h1>
    <p><strong>Protocol:</strong> Upload → Process → Download Response</p>
    <p><strong>Status:</strong> Running ✅</p>
    <p><strong>Response Audio:</strong> {'✅ Found' if audio_exists else '❌ Not Found'}</p>
    {f'<p><strong>Response Size:</strong> {audio_size/1024:.2f} KB</p>' if audio_exists else ''}
    <hr>
    <p><em>💡 Place your response audio file as "response_audio.wav" in the server directory</em></p>
    '''

if __name__ == '__main__':
    print("=" * 60)
    print("🚀 WebSocket Audio Server (BIDIRECTIONAL)")
    print("=" * 60)
    print(f"📁 Upload folder: {os.path.abspath(UPLOAD_FOLDER)}")
    print(f"🌐 Endpoint: ws://0.0.0.0:5000/upload")
    print(f"📋 Protocol: UPLOAD → DOWNLOAD")
    
    if os.path.exists(RESPONSE_AUDIO):
        size = os.path.getsize(RESPONSE_AUDIO)
        print(f"📤 Response audio: {RESPONSE_AUDIO} ({size/1024:.2f} KB) ✅")
    else:
        print(f"⚠️  No response audio found!")
        print(f"💡 Create: {os.path.abspath(RESPONSE_AUDIO)}")
    
    print("=" * 60 + "\n")
    
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)