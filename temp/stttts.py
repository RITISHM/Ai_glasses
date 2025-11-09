from faster_whisper import WhisperModel
import torch
import pyttsx3


# Load the model python s 
# arge-v2 is high quality; choose smaller models for speed)
model_size = "small"
model = WhisperModel(model_size, device="cuda")  # force GPU usage
engine = pyttsx3.init()
voices = engine.getProperty('voices')
for i in voices:
  print(i)
engine.setProperty('voice','HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_HI-IN_HEMANT_11.0')  # e.g., 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\TTS_MS_HI-IN_Hemant_11.0'
engine.setProperty('rate', 150)  # Default is ~200

# Set volume (0.0 to 1.0)
engine.setProperty('volume', 0.9)
# List all voices
voices = engine.getProperty('voice')
print (voices)
# Path to your audio file (WAV or MP3)

engine.say(""" धूप की किरणें,
मन को छू जाएँ।
खुशियों की बहार, hi my name is """)
engine.runAndWait()