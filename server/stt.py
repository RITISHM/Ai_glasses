from faster_whisper import WhisperModel
import torch


# Load the model python s 
# arge-v2 is high quality; choose smaller models for speed)
model_size = "small"
model = WhisperModel(model_size, device="cuda")  # force GPU usage

# # Path to your audio file (WAV or MP3)
# audio_path = "indian_accent.mp3"
# for i in range(5):
# # Transcribe
#     segments, info = model.transcribe(audio_path, beam_size=10,language="en")


#     for segment in segments: