from .api import end_chat, start_chat, generate_image_response, generate_prompt_response
from .stt import speech_to_text
from .tts import text_to_speech

print("Transcribing audio...")
transcribe =speech_to_text(r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\temp\indian_accent.mp3")
print ("🗣️ Transcribed:", transcribe)
print("Generating AI response...")
response_text = generate_prompt_response( transcribe)
print(f"💬 Response: {response_text}")

print("Converting to speech...")
text_to_speech(response_text, r"C:\Users\RITISH\OneDrive\Desktop\projects\iot_aiglasses\coding_story.mp3")
