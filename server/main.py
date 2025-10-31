from flask import Flask, request, Response
from werkzeug.exceptions import RequestEntityTooLarge
import os
from dotenv import load_dotenv
from .api import end_chat, start_chat, generate_image_response, generate_prompt_response
from .stt import speech_to_text
from .tts import text_to_speech
from datetime import datetime

load_dotenv()
UPLOAD_FOLDER = os.getenv("UPLOAD_FOLDER")
app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 32 * 1024 * 1024  # 32MB max

AUDIO_FOLDER = os.path.join(UPLOAD_FOLDER, 'audio')
IMAGE_FOLDER = os.path.join(UPLOAD_FOLDER, 'images')
HISTORY_FOLDER = os.path.join(UPLOAD_FOLDER, 'chat_history')
RESPONSE_FOLDER = os.path.join(r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\uploads", 'response')

INDEX = 0
CHAT_STARTED = False

# Ensure directories exist
for folder in [AUDIO_FOLDER, IMAGE_FOLDER, HISTORY_FOLDER, RESPONSE_FOLDER]:
    os.makedirs(folder, exist_ok=True)


@app.errorhandler(RequestEntityTooLarge)
def handle_large_file(e):
    return "File too large. Maximum size is 32MB.", 413


@app.route('/status')
def status():
    return "All good", 200


@app.route('/imageresponse', methods=["POST"])
def image_response():
    global INDEX, CHAT_STARTED

    print("\n=== Image Response Request ===")

    if "audio" not in request.files or "image" not in request.files:
        return "Missing required files", 400

    audio_file = request.files["audio"]
    image_file = request.files["image"]

    # Save uploaded files
    audio_path = os.path.join(AUDIO_FOLDER, f"audio_{INDEX}.wav")
    image_path = os.path.join(IMAGE_FOLDER, f"image_{INDEX}.jpg")

    audio_file.save(audio_path)
    image_file.save(image_path)
    print(f"✓ Audio saved: {audio_path} ({os.path.getsize(audio_path)} bytes)")
    print(f"✓ Image saved: {image_path} ({os.path.getsize(image_path)} bytes)")

    response_path = os.path.join(RESPONSE_FOLDER, f"response_{INDEX}.wav")

    # Start chat if needed
    if not CHAT_STARTED:
        print("Starting chat session...")
        start_chat()
        CHAT_STARTED = True

    # Transcribe and generate response
    print("Transcribing audio...")
    transcribe =speech_to_text(audio_path)
    print ("🗣️ Transcribed:", transcribe)
    print("Generating AI response...")
    response_text = generate_image_response(image_path, transcribe)
    print(f"💬 Response: {response_text}")

    print("Converting to speech...")
    text_to_speech(response_text, response_path)
    print(f"🔊 Audio response saved: {response_path}")

    # Validate output file
    if not os.path.exists(response_path):
        return "Error: Response audio not found", 500

    if os.path.getsize(response_path) == 0:
        return "Error: Empty response file", 500

    # Increment for next request
    INDEX += 1

    # Manual chunked stream response
    def generate():
        with open(response_path, "rb") as f:
            while chunk := f.read(4096):  # 4KB chunks
                yield chunk

    print(f"🚀 Streaming {response_path} to client...")
    return Response(
        generate(),
        mimetype='audio/wav',
        headers={
            "Cache-Control": "no-cache",
            "Content-Type": "audio/wav",
            "Transfer-Encoding": "chunked",
            "Connection": "keep-alive",
            "Accept-Ranges": "bytes"
        }
    )


@app.route('/audioresponse', methods=["POST"])
def audio_response():
    global INDEX, CHAT_STARTED

    print("\n=== Audio Response Request ===")

    if "audio" not in request.files:
        return "No audio file part", 400

    audio_file = request.files["audio"]

    audio_path = os.path.join(AUDIO_FOLDER, f"audio_{INDEX}.wav")
    response_path = os.path.join(RESPONSE_FOLDER, f"response_{INDEX}.mp3")

    audio_file.save(audio_path)
    print(f"✓ Audio saved: {audio_path}")

    if not CHAT_STARTED:
        start_chat()
        CHAT_STARTED = True

    print("Transcribing...")
    transcribe = speech_to_text(audio_path)
    print(f"🗣️ Transcribed: {transcribe}")

    response_text = generate_prompt_response(transcribe)
    print(f"💬 Response: {response_text}")

    text_to_speech(response_text,r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\uploads\response\response_0.wav")
    

    if not os.path.exists(r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\uploads\response\response_0.wav"):
        return "Error: Response not created", 500

    if os.path.getsize(r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\uploads\response\response_0.wav") == 0:
        return "Error: Empty response audio", 500

    INDEX += 1

    def generate():
        with open(r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\uploads\response\response_0.wav", "rb") as f:
            while chunk := f.read(4096):
                yield chunk

    print(f"🚀 Streaming {response_path} to client...")
    return Response(
        generate(),
        mimetype='audio/wav',
        headers={
            "Cache-Control": "no-cache",
            "Content-Type": "audio/wav",
            "Transfer-Encoding": "chunked",
            "Connection": "keep-alive",
            "Accept-Ranges": "bytes"
        }
    )


@app.route('/end-conversation')
def end_conversation():
    global CHAT_STARTED, INDEX
    end_chat(HISTORY_FOLDER)
    CHAT_STARTED = False
    INDEX = 0
    return "Chat ended and saved", 200


if __name__ == '__main__':
    print("Starting Flask server...")
    print(f"Audio folder: {AUDIO_FOLDER}")
    print(f"Image folder: {IMAGE_FOLDER}")
    print(f"Response folder: {RESPONSE_FOLDER}")

