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
text="""Create a parent class that takes the name and age of a person. It should include a method called greet, which prints the person's name and age. Then, define a child class that inherits the properties of the parent class. This child class should override the greet method to include additional friendly phrases, such as 'Hello!' and 'Good morning!', followed by the person's name and age.
"""
engine.save_to_file(text, "output.wav")
engine.runAndWait()