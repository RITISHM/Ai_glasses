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
    print('✅ Client connected via WebSocket')
    
    try:
        # Receive binary data
        data = ws.receive()
        
        if data:
            t0 = time.time()
            filename = f"{UPLOAD_FOLDER}/audio_{int(time.time())}.wav"
            
            # Write binary data to file
            with open(filename, "wb") as f:
                f.write(data)
            
            elapsed = time.time() - t0
            print(f"✅ Saved {filename} ({len(data)} bytes) in {elapsed:.2f}s")
            
            # Send response back
            response = f'{{"status": "ok", "saved_as": "{filename}", "server_time": {elapsed:.2f}, "size": {len(data)}}}'
            ws.send(response)
        else:
            print("⚠️ Received empty data")
            ws.send('{"status": "error", "message": "No data received"}')
        
    except Exception as e:
        print(f"❌ Error: {e}")
        try:
            ws.send(f'{{"status": "error", "message": "{str(e)}"}}')
        except:
            pass
    
    finally:
        print('🔌 Client disconnected')

@app.route('/')
def index():
    return '''
    <h1>WebSocket Audio Upload Server</h1>
    <p>WebSocket endpoint: ws://YOUR_IP:5000/upload</p>
    <p>Status: Running ✅</p>
    '''

if __name__ == '__main__':
    print("=" * 50)
    print("🚀 WebSocket Audio Upload Server")
    print("=" * 50)
    print(f"📁 Upload folder: {os.path.abspath(UPLOAD_FOLDER)}")
    print(f"🌐 WebSocket endpoint: ws://0.0.0.0:5000/upload")
    print(f"🌐 HTTP test page: http://0.0.0.0:5000/")
    print("=" * 50)
    app.run(host='0.0.0.0', port=5000, debug=True)