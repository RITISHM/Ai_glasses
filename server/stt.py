from faster_whisper import WhisperModel
import torch


# Load the model python stt
model_size = "small"
stt_model = WhisperModel(model_size, device="cuda")  # force GPU usage
print ("STT setup is done ✅")

def speech_to_text(input_audio_path):
   segments, info = stt_model.transcribe(input_audio_path, beam_size=10,)
   text = " , ".join([seg.text for seg in segments])
   return text