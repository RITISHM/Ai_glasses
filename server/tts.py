import pyttsx3

engine = pyttsx3.init()

# List all voices
voices = engine.getProperty('voices')
for voice in voices:
    print(voice.id)

# Set Hindi voice (replace with actual ID from above)
engine.setProperty('voice', voices[0])  # e.g., 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\TTS_MS_HI-IN_Hemant_11.0'
engine.setProperty('rate', 150)  # Default is ~200

# Set volume (0.0 to 1.0)
engine.setProperty('volume', 0.9)