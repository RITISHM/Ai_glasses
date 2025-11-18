from flask import Flask, send_from_directory, render_template
from flask_sock import Sock
import time
import os
import json
import traceback
from threading import Lock
from collections import deque

# Import your existing functions
from .api import end_chat, start_chat, generate_image_response, generate_prompt_response
from .stt import speech_to_text
from .tts import text_to_speech

app = Flask(__name__)
sock = Sock(app)

UPLOAD_FOLDER = "uploads"
AUDIO_FOLDER = os.path.join(UPLOAD_FOLDER, "audio")
IMAGE_FOLDER = os.path.join(UPLOAD_FOLDER, "images")
RESPONSE_FOLDER = os.path.join(UPLOAD_FOLDER, "response")

os.makedirs(AUDIO_FOLDER, exist_ok=True)
os.makedirs(IMAGE_FOLDER, exist_ok=True)
os.makedirs(RESPONSE_FOLDER, exist_ok=True)

# Lock for thread safety
upload_lock = Lock()

# Global message queue for broadcasting
message_queue = deque(maxlen=100)
broadcast_clients = set()

# OPTIMIZATION: Increased chunk sizes for faster transfer
RECEIVE_CHUNK_SIZE = 32768  # 32KB chunks
SEND_CHUNK_SIZE = 1024*33     # 32KB chunks
chat_started=False
def send_error_response(ws, message):
    """Send error response to client"""
    try:
        error_msg = json.dumps({"status": "error", "message": message})
        ws.send(error_msg)
    except Exception as e:
        print(f"Failed to send error: {e}")

def broadcast_to_clients(message):
    """Send message to all connected web clients"""
    message_queue.append(message)
    disconnected = set()
    
    for client in broadcast_clients:
        try:
            client.send(json.dumps(message))
        except Exception as e:
            print(f"Failed to send to client: {e}")
            disconnected.add(client)
    
    # Remove disconnected clients
    broadcast_clients.difference_update(disconnected)

def verify_wav_header(data):
    """Verify if data starts with valid WAV header"""
    if len(data) < 44:
        return False
    
    if data[0:4] != b'RIFF':
        return False
    
    if data[8:12] != b'WAVE':
        return False
    
    return True

def verify_jpeg_header(data):
    """Verify if data starts with valid JPEG header"""
    if len(data) < 2:
        return False
    
    # JPEG starts with FF D8
    if data[0:2] != b'\xff\xd8':
        return False
    
    return True

@sock.route('/broadcast')
def broadcast(ws):
    """WebSocket endpoint for web clients to receive updates"""
    broadcast_clients.add(ws)
    print(f"✅ Web client connected. Total clients: {len(broadcast_clients)}")
    
    try:
        # Send historical messages
        for msg in message_queue:
            ws.send(json.dumps(msg))
        
        # Keep connection alive
        while True:
            data = ws.receive(timeout=30)
            if data is None:
                break
    except Exception as e:
        print(f"Broadcast client error: {e}")
    finally:
        broadcast_clients.discard(ws)
        print(f"🔌 Web client disconnected. Remaining: {len(broadcast_clients)}")

@sock.route('/upload')
def upload(ws):
    print('=' * 50)
    print('✅ Client connected')
    print('=' * 50)
  
    with upload_lock:
        chunks = []
        total_received = 0
        expected_image_size = 0
        expected_audio_size = 0
        image_filename = None
        audio_filename = None
        
        try:
            # ===== RECEIVE METADATA (image_size,audio_size) =====
            metadata_msg = ws.receive(timeout=10)
            
            if not metadata_msg:
                print("❌ No metadata received")
                send_error_response(ws, "No metadata received")
                return
            
            try:
                parts = metadata_msg.split(',')
                if len(parts) != 2:
                    raise ValueError("Invalid metadata format")
                
                expected_image_size = int(parts[0])
                expected_audio_size = int(parts[1])
                
                print(f"📦 Expecting: Image={expected_image_size/1024:.1f} KB, Audio={expected_audio_size/1024:.1f} KB")
                
                if expected_audio_size <= 0 or expected_audio_size > 10000000:
                    print(f"❌ Invalid audio size: {expected_audio_size}")
                    send_error_response(ws, "Invalid audio size")
                    return
                    
            except (ValueError, IndexError) as e:
                print(f"❌ Invalid metadata format: {metadata_msg}")
                send_error_response(ws, "Invalid metadata format")
                return
            
            # ===== RECEIVE IMAGE (if size > 0) =====
            image_data = None
            image_time = 0
            if expected_image_size > 0:
                print(f'📥 Receiving image...')
                t_image_start = time.time()
                
                image_chunks = []
                image_received = 0
                
                time.sleep(0.1)
                
                while image_received < expected_image_size:
                    try:
                        chunk_timeout = 10
                        data = ws.receive(timeout=chunk_timeout)
                        
                        if data is None:
                            print(f"⚠️ Image receive timeout")
                            break
                        
                        if isinstance(data, str):
                            print(f"⚠️ Unexpected text during image: {data[:50]}")
                            continue
                        
                        if isinstance(data, bytes) and len(data) > 0:
                            image_chunks.append(data)
                            image_received += len(data)
                            
                            if image_received >= expected_image_size:
                                break
                    
                    except Exception as e:
                        print(f"⚠️ Image receive error: {e}")
                        break
                
                if image_chunks:
                    image_data = b''.join(image_chunks)
                    image_time = time.time() - t_image_start
                    print(f"📦 Image received: {len(image_data)/1024:.1f} KB in {image_time:.1f}s")
                    
                    if not verify_jpeg_header(image_data):
                        print("⚠️ Invalid JPEG header")
                    else:
                        print("✅ Valid JPEG detected")
            
            # ===== RECEIVE AUDIO =====
            print(f'📥 Receiving audio...')
            t_audio_start = time.time()
            
            audio_chunks = []
            audio_received = 0
            chunk_count = 0
            
            time.sleep(0.1)
            
            while True:
                try:
                    remaining = expected_audio_size - audio_received
                    
                    if chunk_count == 0:
                        chunk_timeout = 15
                    elif remaining > 50000:
                        chunk_timeout = 10
                    elif remaining > 0:
                        chunk_timeout = 5
                    else:
                        break
                    
                    data = ws.receive(timeout=chunk_timeout)
                    
                    if data is None:
                        print(f"⚠️ Audio timeout after {chunk_count} chunks")
                        if audio_received > 1000:
                            break
                        else:
                            break
                    
                    if isinstance(data, str):
                        if data == "EOF":
                            print(f"✅ EOF received")
                            break
                        else:
                            continue
                    
                    if isinstance(data, bytes) and len(data) > 0:
                        audio_chunks.append(data)
                        audio_received += len(data)
                        chunk_count += 1
                        
                        if chunk_count % 10 == 0:
                            progress = (audio_received / expected_audio_size) * 100
                            print(f"  📦 Chunk {chunk_count}: {audio_received/1024:.1f} KB / {expected_audio_size/1024:.1f} KB ({progress:.1f}%)")
                        
                        if audio_received >= expected_audio_size:
                            break
                
                except Exception as e:
                    print(f"⚠️ Audio receive error: {e}")
                    if audio_received > 1000:
                        break
                    else:
                        raise
            
            audio_time = time.time() - t_audio_start
            
            if audio_received == 0:
                print("❌ No audio data received")
                send_error_response(ws, "No audio data received")
                return
            
            audio_data = b''.join(audio_chunks)
            print(f"📦 Audio received: {len(audio_data)/1024:.1f} KB in {audio_time:.1f}s ({len(audio_data)/audio_time/1024:.1f} KB/s)")
            
            if not verify_wav_header(audio_data):
                print("⚠️ Invalid WAV header")
            else:
                print("✅ Valid WAV header detected")
            
            # ===== SAVE FILES =====
            timestamp = int(time.time())
            
            # Save image - FIXED: Use just filename, not full path
            if image_data and len(image_data) > 0:
                image_filename = f"image_{timestamp}.jpg"
                image_filepath = os.path.join(IMAGE_FOLDER, image_filename)
                try:
                    with open(image_filepath, "wb") as f:
                        f.write(image_data)
                        f.flush()
                        os.fsync(f.fileno())
                    
                    if os.path.exists(image_filepath):
                        print(f"💾 Image saved: {image_filepath} ({os.path.getsize(image_filepath)/1024:.1f} KB)")
                    else:
                        print(f"⚠️ Image save verification failed")
                        image_filename = None
                        
                except Exception as e:
                    print(f"❌ Image save error: {e}")
                    image_filename = None
            
            # Save audio - FIXED: Use just filename, not full path
            audio_filename = f"audio_{timestamp}.wav"
            audio_filepath = os.path.join(AUDIO_FOLDER, audio_filename)
            try:
                with open(audio_filepath, "wb") as f:
                    f.write(audio_data)
                    f.flush()
                    os.fsync(f.fileno())
                
                if os.path.exists(audio_filepath):
                    print(f"💾 Audio saved: {audio_filepath} ({os.path.getsize(audio_filepath)/1024:.1f} KB)")
                else:
                    print(f"❌ Audio save verification failed")
                    send_error_response(ws, "Audio save failed")
                    return
                    
            except Exception as e:
                print(f"❌ Audio save error: {e}")
                send_error_response(ws, f"Audio save error: {str(e)}")
                return
            
            # FIXED: Response audio filename
            response_filename = f"response_{timestamp}.wav"
            RESPONSE_AUDIO = os.path.join(RESPONSE_FOLDER, response_filename)
            
            # ===== PROCESS AUDIO AND IMAGE =====
            print(f"🤖 Processing audio and image...")
            processing_start = time.time()
            global chat_started
            if not chat_started:
                print("🚀 Starting new chat session...")
                start_chat()
                chat_started = True
            try:
                # Transcribe audio
                transcribe = speech_to_text(audio_filepath)
                print(f"📝 Transcription: {transcribe[:100]}...")
                
                # BROADCAST TRANSCRIPTION TO WEB CLIENTS - FIXED: Use correct URL format
                image_url = f"/images/{image_filename}" if image_filename else None
                print(f"🖼️ Broadcasting image URL: {image_url}")
                if image_filename:
                    image_full_path_check = os.path.join(IMAGE_FOLDER, image_filename)
                    print(f"   File exists: {os.path.exists(image_full_path_check)}")
                    print(f"   Absolute path: {os.path.abspath(image_full_path_check)}")
                broadcast_to_clients({
                    "type": "transcription",
                    "transcription": transcribe,
                    "image_url": image_url,
                    "timestamp": time.time()
                })
                
                # Generate response with image (if available)
                if image_filename and os.path.exists(os.path.join(IMAGE_FOLDER, image_filename)):
                    image_full_path = os.path.join(IMAGE_FOLDER, image_filename)
                    print(f"🖼️ Processing with image context: {image_full_path}")
                    response_text = generate_image_response(image_full_path, transcribe)
                else:
                    print(f"💬 Processing text only...")
                    response_text = generate_prompt_response(transcribe)
                
                print(f"💬 Response: {response_text[:100]}...")
                
                # Convert to speech
                text_to_speech(response_text, RESPONSE_AUDIO)
                
                # Verify the response audio was created
                if not os.path.exists(RESPONSE_AUDIO):
                    print(f"⚠️ Warning: Response audio file not created at {RESPONSE_AUDIO}")
                else:
                    audio_file_size = os.path.getsize(RESPONSE_AUDIO)
                    print(f"✅ Response audio created: {RESPONSE_AUDIO} ({audio_file_size/1024:.1f} KB)")
                
                # Small delay to ensure file is fully written
                time.sleep(0.2)
                
                processing_time = time.time() - processing_start
                print(f"✅ Processing complete ({processing_time:.1f}s)")
                
                # BROADCAST RESPONSE TO WEB CLIENTS - FIXED: Use correct URL format
                audio_url = f"/audio/{response_filename}"
                print(f"🔊 Broadcasting audio URL: {audio_url}")
                print(f"   File exists: {os.path.exists(RESPONSE_AUDIO)}")
                print(f"   Absolute path: {os.path.abspath(RESPONSE_AUDIO)}")
                broadcast_to_clients({
                    "type": "response",
                    "response_text": response_text,
                    "audio_url": audio_url,
                    "timestamp": time.time()
                })
                
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
                    "upload_size": len(audio_data),
                    "image_received": image_filename is not None,
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
                "upload_size": len(audio_data),
                "image_received": image_filename is not None,
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
                        
                        time.sleep(0.01)
                
                send_time = time.time() - send_start
                print(f"✅ Response sent: {sent_bytes/1024:.1f} KB in {send_time:.1f}s ({sent_bytes/send_time/1024:.1f} KB/s)")
                time.sleep(0.7)
                
            except Exception as e:
                print(f"❌ Send error: {e}")
                return
            
            # ===== SUMMARY =====
            total_time = time.time() - t_audio_start
            print(f"✅ Transaction complete ({total_time:.1f}s total)")
            print(f"   Image: {image_time:.1f}s, Audio: {audio_time:.1f}s, Process: {processing_time:.1f}s, Send: {send_time:.1f}s")
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

@app.route('/chat')
def chat():
    """Serve the chat interface"""
    return render_template('chat.html')

@app.route('/images/<filename>')
def serve_image(filename):
    """Serve uploaded images"""
    filepath = os.path.join(IMAGE_FOLDER, filename)
    print(f"🔍 Image request: {filename}")
    print(f"   Full path: {os.path.abspath(filepath)}")
    print(f"   Exists: {os.path.exists(filepath)}")
    
    if os.path.exists(filepath):
        print(f"   ✅ Serving image from {IMAGE_FOLDER}")
        return send_from_directory(os.path.abspath(IMAGE_FOLDER), filename, mimetype='image/jpeg')
    else:
        print(f"   ❌ Image not found")
        # List files in directory for debugging
        if os.path.exists(IMAGE_FOLDER):
            files = os.listdir(IMAGE_FOLDER)
            print(f"   Available files: {files[:5]}")
        return {"error": "Image not found"}, 404

@app.route('/audio/<filename>')
def serve_audio(filename):
    """Serve audio response files"""
    response_path = os.path.join(RESPONSE_FOLDER, filename)
    audio_path = os.path.join(AUDIO_FOLDER, filename)
    
    print(f"🔍 Audio request: {filename}")
    print(f"   Checking response folder: {os.path.abspath(response_path)} (exists: {os.path.exists(response_path)})")
    print(f"   Checking audio folder: {os.path.abspath(audio_path)} (exists: {os.path.exists(audio_path)})")
    
    if os.path.exists(response_path):
        print(f"   ✅ Found in response folder, serving...")
        return send_from_directory(os.path.abspath(RESPONSE_FOLDER), filename, mimetype='audio/wav')
    elif os.path.exists(audio_path):
        print(f"   ✅ Found in audio folder, serving...")
        return send_from_directory(os.path.abspath(AUDIO_FOLDER), filename, mimetype='audio/wav')
    else:
        print(f"   ❌ Audio not found in either location")
        # List files in directory for debugging
        if os.path.exists(RESPONSE_FOLDER):
            files = os.listdir(RESPONSE_FOLDER)
            print(f"   Available response files: {files[:5]}")
        return {"error": "Audio file not found"}, 404

# Legacy routes for backward compatibility
@app.route('/uploads/response/<filename>')
def serve_response_legacy(filename):
    """Legacy route for response audio files"""
    print(f"🔄 Legacy audio request: /uploads/response/{filename}")
    return serve_audio(filename)

@app.route('/uploads/audio/<filename>')
def serve_audio_legacy(filename):
    """Legacy route for audio files"""
    print(f"🔄 Legacy audio request: /uploads/audio/{filename}")
    return serve_audio(filename)

@app.route('/uploads/images/<filename>')
def serve_images_legacy(filename):
    """Legacy route for images"""
    print(f"🔄 Legacy image request: /uploads/images/{filename}")
    return serve_image(filename)

@app.route('/')
def index():
    upload_count = len([f for f in os.listdir(AUDIO_FOLDER) if f.endswith('.wav')]) if os.path.exists(AUDIO_FOLDER) else 0
    image_count = len([f for f in os.listdir(IMAGE_FOLDER) if f.endswith('.jpg')]) if os.path.exists(IMAGE_FOLDER) else 0
    response_count = len([f for f in os.listdir(RESPONSE_FOLDER) if f.endswith('.wav')]) if os.path.exists(RESPONSE_FOLDER) else 0
    
    return f'''<!DOCTYPE html>
<html>
<head>
    <title>Audio & Image WebSocket Server</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }}
        .container {{ background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }}
        h1 {{ color: #333; }}
        .status {{ padding: 10px; border-radius: 5px; margin: 10px 0; }}
        .ok {{ background: #d4edda; color: #155724; }}
        .info {{ background: #d1ecf1; color: #0c5460; }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        td {{ padding: 10px; border-bottom: 1px solid #ddd; }}
        td:first-child {{ font-weight: bold; width: 200px; }}
        .btn {{ display: inline-block; padding: 12px 24px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
                color: white; text-decoration: none; border-radius: 8px; margin: 10px 5px; font-weight: bold; }}
        .btn:hover {{ opacity: 0.9; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🎤📸 Audio & Image WebSocket Server</h1>
        <div class="status ok"><strong>Status:</strong> Running ✅</div>
        <div style="margin: 20px 0;"><a href="/chat" class="btn">🚀 Open Chat Interface</a></div>
        <table>
            <tr><td>WebSocket Endpoint</td><td><code>ws://0.0.0.0:5000/upload</code></td></tr>
            <tr><td>Chat Interface</td><td><a href="/chat" style="color: #667eea;">http://0.0.0.0:5000/chat</a></td></tr>
            <tr><td>Protocol</td><td>Image + Audio → Process → Response</td></tr>
            <tr><td>Chunk Size</td><td>{SEND_CHUNK_SIZE/1024:.0f} KB (Optimized)</td></tr>
            <tr><td>Audio Files</td><td>{upload_count} recordings</td></tr>
            <tr><td>Image Files</td><td>{image_count} images</td></tr>
            <tr><td>Response Files</td><td>{response_count} responses</td></tr>
            <tr><td>Audio Folder</td><td>{os.path.abspath(AUDIO_FOLDER)}</td></tr>
            <tr><td>Image Folder</td><td>{os.path.abspath(IMAGE_FOLDER)}</td></tr>
            <tr><td>Response Folder</td><td>{os.path.abspath(RESPONSE_FOLDER)}</td></tr>
        </table>
        <div class="status info">
            <strong>💡 Features:</strong><br>
            • Captures image from OV2640 camera<br>
            • Records audio after image capture<br>
            • Uses image context for AI response generation<br>
            • Returns audio response to device<br>
            • Real-time chat interface for monitoring<br>
            • 32KB chunk sizes for optimal transfer speed
        </div>
    </div>
</body>
</html>'''

@app.route('/health')
def health():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "timestamp": time.time(),
        "audio_folder": AUDIO_FOLDER,
        "image_folder": IMAGE_FOLDER,
        "response_folder": RESPONSE_FOLDER,
        "broadcast_clients": len(broadcast_clients),
        "optimizations": {
            "receive_chunk_size": RECEIVE_CHUNK_SIZE,
            "send_chunk_size": SEND_CHUNK_SIZE
        }
    }

if __name__ == '__main__':
    print("\n" + "=" * 60)
    print("🚀 Audio & Image WebSocket Server (OPTIMIZED)")
    print("=" * 60)
    print(f"📁 Audio folder: {os.path.abspath(AUDIO_FOLDER)}")
    print(f"🖼️  Image folder: {os.path.abspath(IMAGE_FOLDER)}")
    print(f"📤 Response folder: {os.path.abspath(RESPONSE_FOLDER)}")
    print(f"🌐 WebSocket: ws://0.0.0.0:5000/upload")
    print(f"🌐 Web interface: http://0.0.0.0:5000")
    print(f"💬 Chat interface: http://0.0.0.0:5000/chat")
    print(f"🏥 Health check: http://0.0.0.0:5000/health")
    print(f"📋 Protocol: IMAGE + AUDIO → PROCESS → RESPONSE")
    print(f"⚡ Chunk size: {SEND_CHUNK_SIZE/1024:.0f} KB")
    print("=" * 60 + "\n")
    print("Press Ctrl+C to stop the server\n")
    
    try:
        app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
    except KeyboardInterrupt:
        print("\n\n👋 Server stopped gracefully")