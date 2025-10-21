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

engine.say(""" Aayudhya mein nirmit hune ja rahe Ram Mandir ke chaah pravi ishtwal travid shailge Gopuram ke roop mein banaye jaayin. ,  Saathi pratek Gopuram ka naam karan pishi Ram ke jeevan mein mohatapoon bhoomi ka rakne wale logon ke naam par rakha jaayin. ,  Ge prasta positive India Foundation ke mukhya nyasi eva metihas pura tato ke gata dr Pradeep Dixit ne bhi jaayin. ,  Making a home means making decisions, lots of them. ,  So we promise to be here with prices you will love. ,  If you just promise to put your heart into it, seasons change but our lowest price promise every day. ,  Shop fall at low ease today. ,  At Domino's you get it more than just your favorite pizza. ,  You can get especially chicken savoury pasta oven baked parmesan bread bite, ,  molten lava crunch cakes and yes, medium to toppings pizza as well. ,  Carry out any two or more mix and match items for $5.99 each or get them delivered for $6.99 each.""")
engine.runAndWait()