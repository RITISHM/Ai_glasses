from faster_whisper import WhisperModel
import torch
import pyttsx3


# Load the model python s 
# arge-v2 is high quality; choose smaller models for speed)
model_size = "small"
model = WhisperModel(model_size, device="cuda")  # force GPU usage
engine = pyttsx3.init()
voices = engine.getProperty('voices')
engine.setProperty('voice', voices[0])  # e.g., 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\TTS_MS_HI-IN_Hemant_11.0'
engine.setProperty('rate', 150)  # Default is ~200

# Set volume (0.0 to 1.0)
engine.setProperty('volume', 0.9)
# List all voices
voices = engine.getProperty('voices')
# Path to your audio file (WAV or MP3)
audio_path = "indian_accent.mp3"

for i in range(5):
# Transcribe
    segments, info = model.transcribe(audio_path, beam_size=10,language="en")
    text=text = " , ".join([seg.text for seg in segments])
    print(text)
    engine.say(text)
    engine.runAndWait()