from gtts import gTTS
from pydub import AudioSegment

text = "Hello Ritish, this is a test using gTTS!"

print ("TTS setup is done ✅")

def text_to_speech(text,response_audio_path):
  tts = gTTS(text=text, lang='hi',slow=False)
  tts.save("output.mp3")
  sound = AudioSegment.from_mp3("output.mp3")
  sound = sound.set_frame_rate(16000).set_sample_width(2)
  sound.export(response_audio_path, format="wav")

  return 
   