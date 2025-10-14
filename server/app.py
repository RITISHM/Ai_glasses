from .stt import model
from .tts import engine
def run():
  audio_path = "indian_accent.mp3"

  for i in range(5):
  # Transcribe
      segments, info = model.transcribe(audio_path, beam_size=10,language="en")
      text=text = " , ".join([seg.text for seg in segments])
      engine.say(text)
      engine.runAndWait()
      print(text)