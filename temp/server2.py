#!/usr/bin/env python3
"""
Simple Flask Server for ESP32 Audio-Image Data Reception
"""

from flask import Flask, request, jsonify
import os
from datetime import datetime

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 32 * 1024 * 1024  # 32MB max

# Create upload directories
UPLOAD_FOLDER = 'uploads'
AUDIO_FOLDER = os.path.join(UPLOAD_FOLDER, 'audio')
IMAGE_FOLDER = os.path.join(UPLOAD_FOLDER, 'images')

for folder in [UPLOAD_FOLDER, AUDIO_FOLDER, IMAGE_FOLDER]:
    os.makedirs(folder, exist_ok=True)

@app.route('/upload', methods=['POST'])
def upload_files():
    """Upload audio and image files from ESP32"""
    try:
        # Check if files are present
        if 'audio' not in request.files or 'image' not in request.files:
            return jsonify({'error': 'Both audio and image files required'}), 400
        
        audio_file = request.files['audio']
        image_file = request.files['image']
        
        if audio_file.filename == '' or image_file.filename == '':
            return jsonify({'error': 'No files selected'}), 400
        
        # Generate timestamp for unique filenames
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # Save audio file
        audio_filename = f"audio_{timestamp}.wav"
        audio_path = os.path.join(AUDIO_FOLDER, audio_filename)
        audio_file.save(audio_path)
        audio_size = os.path.getsize(audio_path)
        
        # Save image file
        image_ext = '.jpg' if image_file.filename.lower().endswith(('.jpg', '.jpeg')) else '.png'
        image_filename = f"image_{timestamp}{image_ext}"
        image_path = os.path.join(IMAGE_FOLDER, image_filename)
        image_file.save(image_path)
        image_size = os.path.getsize(image_path)
        
        print(f"Uploaded: {audio_filename} ({audio_size} bytes), {image_filename} ({image_size} bytes)")
        
        return jsonify({
            'status': 'success',
            'timestamp': timestamp,
            'audio_file': audio_filename,
            'image_file': image_filename,
            'total_size_mb': round((audio_size + image_size) / (1024 * 1024), 2)
        }), 200
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/status', methods=['GET'])
def status():
    """Server check endpoint"""
    return jsonify({
        'status': 'running',
        'message': 'ESP32 audio-image receiver ready',
        'timestamp': datetime.now().isoformat()
    }), 200

@app.route('/files', methods=['GET'])
def list_files():
    """List uploaded files"""
    try:
        audio_files = [f for f in os.listdir(AUDIO_FOLDER) if f.endswith('.wav')] if os.path.exists(AUDIO_FOLDER) else []
        image_files = [f for f in os.listdir(IMAGE_FOLDER) if f.lower().endswith(('.jpg', '.jpeg', '.png'))] if os.path.exists(IMAGE_FOLDER) else []
        
        return jsonify({
            'audio_files': sorted(audio_files, reverse=True),
            'image_files': sorted(image_files, reverse=True),
            'total_files': len(audio_files) + len(image_files)
        }), 200
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.errorhandler(413)
def file_too_large(e):
    return jsonify({'error': 'File too large (max 32MB)'}), 413

if __name__ == '__main__':
    print("ESP32 Audio-Image Server")
    print(f"Audio folder: {os.path.abspath(AUDIO_FOLDER)}")
    print(f"Image folder: {os.path.abspath(IMAGE_FOLDER)}")
    print("Server starting on http://0.0.0.0:5000")
    
    app.run(host='0.0.0.0', port=5000, debug=True)