


<div align="center">

# 🧠 AI Glasses — Real-Time Voice Assistant  
### *with ESP32-S3 Sense & Flask Backend*

![Python](https://img.shields.io/badge/Python-3.10+-blue.svg?style=flat&logo=python)
![Flask](https://img.shields.io/badge/Flask-Backend-black.svg?style=flat&logo=flask)
![ESP32](https://img.shields.io/badge/ESP32--S3-Firmware-orange.svg?style=flat&logo=espressif)
![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat)
![Status](https://img.shields.io/badge/Status-Active-success.svg?style=flat)

> ⚡️ AI-powered wearable assistant that **listens, sees, and speaks** — built for **edge efficiency** and **real-time intelligence**.

</div>

---

## ✨ Overview

This project brings **AI-powered smart glasses** to life using the **ESP32-S3 Sense** board.  
It captures **voice and image data**, streams it to a **Flask backend**, and delivers **spoken responses** in real time using **Pyttsx3** and a **MAX98357 DAC**.

It’s designed for **ultra-low latency**, **modular architecture**, and **ethical edge AI** deployment.

---

## 🔧 Features

- 🎙️ **Voice Capture** via ESP32-S3 MEMS microphone  
- 📷 **Image Capture** using onboard camera  
- 📡 **Audio & Image Streaming** to Flask server  
- 🧠 **Speech-to-Text (ASR)** via FasterWhisper  
- 🤖 **AI Response Generation** using OpenAI API or local model  
- 🔊 **Text-to-Speech (TTS)** using Pyttsx3 (WAV output)  
- 🔁 **Audio Playback** via MAX98357 DAC (I2S)  
- 🧩 **Modular Flask Pipeline** — ASR, LLM, TTS separated cleanly  

---

## 🏗️ Architecture

```

[ESP32-S3 Sense]
├─ Record audio (MEMS mic)
├─ Capture image (camera)
└─ POST to Flask server
├─ Transcribe audio → FasterWhisper
├─ Generate response → LLM API
├─ Convert response to WAV → Pyttsx3
└─ Stream WAV back to ESP32
└─ Playback via MAX98357 DAC (I2S)

```

---

## 📁 Project Structure

```

AI-Glasses/
│
├── esp32/          # Arduino firmware for ESP32-S3
├── server/         # Flask backend with REST endpoints
├── asr/            # FasterWhisper transcription wrapper
├── llm/            # LLM query handler (OpenAI / Local)
├── tts/            # Pyttsx3-based Text-to-Speech module
└── utils/          # Logging, preprocessing, buffer management

````

---

## ⚙️ Setup & Installation

### 🔌 ESP32-S3 Firmware

1. Flash the code in `esp32/main.ino`  
2. Configure **WiFi credentials** and **Flask server IP**  
3. Connect **MAX98357 DAC** to I2S pins  
4. Press the button to trigger **recording + image capture**

---

### 🖥️ Flask Backend Setup

```bash
# 1️⃣ Create virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# 2️⃣ Install dependencies
pip install -r requirements.txt

# 3️⃣ Run Flask server
python server/app.py
````

> 💡 Tip: Use `ngrok` or local tunnel for quick remote testing between your PC and ESP32.

---

## 🔁 Data Flow

| Step | Process                      | Description                          |
| ---- | ---------------------------- | ------------------------------------ |
| 1️⃣  | 🎙️ Audio & 📷 Image Capture | ESP32 records and snaps image        |
| 2️⃣  | 📡 Upload                    | Data sent via HTTP POST to Flask     |
| 3️⃣  | 🧠 Transcription             | FasterWhisper converts speech → text |
| 4️⃣  | 🤖 Response Generation       | LLM creates smart reply              |
| 5️⃣  | 🔊 TTS                       | Pyttsx3 converts text → WAV          |
| 6️⃣  | 🚀 Streaming                 | Flask streams WAV back to ESP32      |
| 7️⃣  | 🔈 Playback                  | ESP32 plays response via DAC         |

---

## 🧪 Testing & Benchmarking

| Test             | Goal                               | Metric                       |
| ---------------- | ---------------------------------- | ---------------------------- |
| ⏱️ Latency       | Measure from audio POST → playback | Time (ms)                    |
| 🗣️ ASR Accuracy | Validate transcription             | WER (Word Error Rate)        |
| 🤖 LLM Quality   | Evaluate response clarity          | Coherence, Context           |
| 🔉 Audio         | Check playback quality             | Subjective & objective tests |

---

## 🗺️ Roadmap

* [ ] 🗣️ Add **Speaker Diarization** for multi-user dialogue
* [ ] 🧠 Integrate **Local LLM** for offline use
* [ ] ⚡ Optimize **Buffer Handling** for XIAO ESP32-S3 (no PSRAM)
* [ ] 🕶️ Develop **Wearable Form Factor** (mini speaker + battery)
* [ ] 👁️ Add **Image-based prompt enrichment** (“Describe this scene”)

---

## 💡 Philosophy

> *“AI should amplify human potential — not replace it.”*

This project embodies:

* 🧩 **Modularity** — each part can evolve independently
* ⚡ **Edge Efficiency** — minimal latency, privacy-first design
* 🤝 **Ethical AI** — transparency, respect, and control

---

## 📜 License

This project is released under the **MIT License** — use, adapt, and improve freely.

---

<div align="center">

### 🌟 Inspiration

> *Combining the worlds of AI, IoT, and wearable tech — transforming the ESP32-S3 into a real-time conversational companion.*

**Made with ❤️ and ☕ by innovators who believe in edge AI.**

</div>
```

