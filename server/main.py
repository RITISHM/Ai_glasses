from flask import Flask ,request,send_file
from werkzeug.exceptions import RequestEntityTooLarge
import os
from dotenv import load_dotenv
from .api import end_chat, start_chat, generate_image_response,generate_prompt_response
from .stt import speech_to_text
from .tts import text_to_speech
from datetime import datetime

load_dotenv()
UPLOAD_FOLDER=os.getenv("UPLOAD_FOLDER")
app=Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 32 * 1024 * 1024  # 32MB max


AUDIO_FOLDER = os.path.join(UPLOAD_FOLDER, 'audio')
IMAGE_FOLDER = os.path.join(UPLOAD_FOLDER, 'images')
HISTORY_FOLDER = os.path.join(UPLOAD_FOLDER, 'chat_history')
RESPONSE_FOLDER=os.path.join(UPLOAD_FOLDER, 'response')
INDEX=0
CHAT_STARTED=False


@app.errorhandler(RequestEntityTooLarge)
def handle_large_file(e):
    return "File too large. Maximum size is 32MB.", 413

@app.route('/status')
def status():
  return "All good",200


@app.route('/image-response',methods=["POST"])
def image_response():
  if "audio" not in request.files:
    return "No audio file part", 400

  if "image" not in request.files:
      return "No image file part", 400
  
  audio_file=request.files["audio"]
  image_file=request.files['image']

  #saving the audio file
  audio_filename = f"audio_{INDEX}.wav"
  audio_path = os.path.join(AUDIO_FOLDER, audio_filename)
  audio_file.save(audio_path)

  #Saving the image file
  image_filename = f"image_{INDEX}.wav"
  image_path = os.path.join(IMAGE_FOLDER, image_filename)
  image_file.save(image_path)
  response_path=os.path.join(RESPONSE_FOLDER,f"response_{INDEX}.wav")

  if not CHAT_STARTED:
    start_chat()
    CHAT_STARTED=True
  transcibe=speech_to_text(audio_path)
  response =generate_image_response(image_path,transcibe)
  text_to_speech(response, response_path)
  INDEX+=1
  return send_file(response_path,mimetype="audio/wav")


@app.route('/audio-response',methods=["POST"])
def audio_response():
  if "audio" not in request.files:
    return "No audio file part", 400
  
  audio_file=request.files["audio"]
  #saving the audio file
  audio_filename = f"audio_{INDEX}.wav"
  audio_path = os.path.join(AUDIO_FOLDER, audio_filename)
  audio_file.save(audio_path)

  response_path=os.path.join(RESPONSE_FOLDER,f"response_{INDEX}.wav")

  if not CHAT_STARTED:
    start_chat()
    CHAT_STARTED=True


  transcibe=speech_to_text(audio_path)
  response =generate_prompt_response(transcibe)
  text_to_speech(response, response_path)
  INDEX+=1
  return send_file(response_path,mimetype="audio/wav")


@app.route('/end-conversation')
def end_conversation():
  end_chat(HISTORY_FOLDER)
  CHAT_STARTED=False
  return "Chat ended and saved",200
