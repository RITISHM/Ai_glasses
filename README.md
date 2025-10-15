🧠 AI Glasses — Real-Time Voice Assistant with ESP32-S3 & Flask

Overview
This project enables AI-powered glasses that capture voice and image data via ESP32-S3 Sense, stream it to a Flask backend, and deliver real-time spoken responses using Pyttsx3 and MAX98357 DAC. It’s designed for ultra-low-latency edge deployment with modular components and robust hardware-software integration.

🔧 Features
- 🎙️ Voice capture via ESP32-S3 Sense MEMS mic  
- 📷 Image capture using onboard camera  
- 📡 Streaming audio/image to Flask backend  
- 🧠 Transcription via FasterWhisper (ASR)  
- 🤖 LLM-powered response generation (OpenAI or local)  
- 🔊 TTS via Pyttsx3 (WAV format)  
- 🔁 Audio streamed back to ESP32 and played via MAX98357 DAC  
- 🧩 Modular Flask pipeline with clean separation of ASR, LLM, and TTS  

🛠️ Architecture
`plaintext
[ESP32-S3 Sense]
 ├─ Record audio (MEMS mic)
 ├─ Capture image (camera)
 └─ POST to Flask server
       ├─ Transcribe audio → FasterWhisper
       ├─ Generate response → LLM API
       ├─ Convert response to WAV → Pyttsx3
       └─ Stream WAV back to ESP32
             └─ Playback via MAX98357 DAC (I2S)
`

📦 Modules
- esp32/: Arduino code for recording, image capture, and streaming  
- server/: Flask app with endpoints for audio/image POST and WAV streaming  
- asr/: FasterWhisper wrapper for transcription  
- llm/: LLM query handler (OpenAI or local model)  
- tts/: Pyttsx3-based TTS module with WAV output  
- utils/: Audio/image preprocessing, logging, buffer management  

🚀 Setup

ESP32-S3
- Flash with esp32/main.ino  
- Configure WiFi credentials and Flask server IP  
- Connect MAX98357 DAC to I2S pins  
- Use button to trigger recording and image capture  

Flask Backend
`bash

Create virtual environment
python -m venv venv
source venv/bin/activate

Install dependencies
pip install -r requirements.txt

Run Flask server
python server/app.py
`

🔁 Data Flow
1. ESP32 records audio and captures image  
2. Sends both to Flask via HTTP POST  
3. Flask transcribes audio using FasterWhisper  
4. Text sent to LLM for response  
5. Response converted to WAV via Pyttsx3  
6. WAV streamed back to ESP32  
7. ESP32 plays response via MAX98357 DAC  

🧪 Testing
- Benchmark latency from audio POST to playback  
- Test transcription accuracy across environments  
- Compare LLM responses with different prompts  
- Validate WAV playback quality on ESP32  

📈 Roadmap
- [ ] Add speaker diarization for multi-user interaction  
- [ ] Integrate local LLM for offline mode  
- [ ] Optimize buffer handling for XIAO ESP32-S3 (no PSRAM)  
- [ ] Add wearable form factor with miniature speaker  
- [ ] Enable image-based prompt enrichment (e.g., describe scene)  

🧠 Philosophy
This system reflects a commitment to modularity, edge efficiency, and ethical AI deployment. Every component is designed for clarity, maintainability, and real-world feedback loops.