from flask import Flask
from flask_sock import Sock
import time
import os

app = Flask(__name__)
sock = Sock(app)

UPLOAD_FOLDER = "uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@sock.route('/upload')
def upload(ws):
    print('✅ Client connected')
    
    try:
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
        
        if chunks:
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
            total_time = t_write_end - t0
            
            size_kb = len(full_data) / 1024
            speed = size_kb / receive_time if receive_time > 0 else 0
            
            print(f"⏱️  Receive time: {receive_time:.3f}s")
            print(f"💾 Write time: {write_time:.3f}s")
            print(f"🚀 Speed: {speed:.2f} KB/s")
            print(f"✅ Saved: {filename}\n")
            
            # Send response
            response = f'{{"status":"ok","size":{len(full_data)},"chunks":{chunk_count},"time":{total_time:.3f},"speed":{speed:.2f}}}'
            ws.send(response)
        else:
            print("⚠️ No data received")
            ws.send('{"status":"error","message":"No data"}')
        
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
    return '''
    <h1>WebSocket Audio Server (Chunked Protocol)</h1>
    <p>Protocol: SIZE → CHUNKS → EOF → RESPONSE</p>
    <p>Status: Running ✅</p>
    '''

if __name__ == '__main__':
    print("=" * 60)
    print("🚀 WebSocket Audio Upload Server (Chunked)")
    print("=" * 60)
    print(f"📁 Upload folder: {os.path.abspath(UPLOAD_FOLDER)}")
    print(f"🌐 Endpoint: ws://0.0.0.0:5000/upload")
    print(f"📋 Protocol: SIZE message → Binary chunks → EOF → Response")
    print("=" * 60 + "\n")
    
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)