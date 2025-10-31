import pyttsx3

tts_model = pyttsx3.init()

# List all voices
voices = tts_model.getProperty('voices')
#Set the voice id ( 0 for male and 1 for female) 
tts_model.setProperty('voice', voices[0])
#Set voice speed
tts_model.setProperty('rate', 200) 
# Set volume (0.0 to 1.0)
tts_model.setProperty('volume', 0.9)
print ("TTS setup is done ✅")

def text_to_speech(text,response_audio_path):
  tts_model.save_to_file(text, response_audio_path)
  tts_model.runAndWait()
  print("doen tts")
  return 
   