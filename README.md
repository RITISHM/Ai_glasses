<div align="center">

# 🧠 **AURALENS**

### _An Intelligent Wearable Assistant that Listens, Sees, and Speaks._

![Python](https://img.shields.io/badge/Python-3.10+-blue.svg?style=for-the-badge&logo=python)
![Flask](https://img.shields.io/badge/Flask-Backend-black.svg?style=for-the-badge&logo=flask)
![ESP32](https://img.shields.io/badge/XIAO--ESP32--S3--SENSE-orange.svg?style=for-the-badge&logo=espressif)
![Gemini](https://img.shields.io/badge/Google-Gemini_API-4285F4?style=for-the-badge&logo=google)
![FasterWhisper](https://img.shields.io/badge/ASR-FasterWhisper-red?style=for-the-badge&logo=openai)
![License](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)

> ⚡ _Auralens brings multimodal AI to the edge — merging voice, vision, and intelligence into one wearable experience._

</div>

---

## 🌟 **Overview**

**Auralens** transforms the **Seeed XIAO ESP32-S3 Sense** into a **smart wearable assistant** powered by on-device sensors and a Flask-based AI backend.  
It captures **audio and images**, sends them to a **Python server** for real-time inference, and returns a **spoken AI response** — right through your glasses.

Built for **speed**, **efficiency**, and **privacy**, Auralens is designed to operate as a **lightweight conversational and visual assistant** running at the edge.

---

## 🎯 **Key Capabilities**

| Category                   | Description                                        |
| -------------------------- | -------------------------------------------------- |
| 🎙️ **Voice Interaction**   | Capture speech through built-in MEMS mic           |
| 📸 **Visual Input**        | Use onboard camera for contextual image capture    |
| 📡 **Smart Communication** | HTTP-based streaming between ESP32-S3 and Flask    |
| 🧠 **AI Understanding**    | FasterWhisper for ASR + Gemini API for reasoning   |
| 🔊 **Response Output**     | Pyttsx3 TTS → Audio playback via MAX98357 DAC      |
| ⚙️ **Dual Touch Control**  | Two-touch interface: ① Voice only, ② Voice + Image |

---

## 🏗️ **System Architecture**

```text
            ┌────────────────────────────────────────┐
            │           👓 AURALENS (ESP32-S3)       │
            │────────────────────────────────────────│
            │ 🎤  Record Audio                       │
            │ 📸  Capture Image                      │
            │ 💾  Store to SD Card                   │
            │ ✋  Dual Touch Controls (Mode Select)  │
            │ 📡  Send Data via HTTP → Flask Backend │
            └──────────────┬──────────────────────────┘
                           │
                           ▼
            ┌────────────────────────────────────────┐
            │        🧠 Flask Backend (Python)       │
            │────────────────────────────────────────│
            │ 🎧  FasterWhisper → Speech-to-Text     │
            │ 🤖  Gemini API → Response Generation   │
            │ 🗣️  Pyttsx3 → Text-to-Speech (WAV)     │
            │ 📤  Return Response.wav → ESP32        │
            └──────────────┬──────────────────────────┘
                           │
                           ▼
            ┌────────────────────────────────────────┐
            │     🔊 ESP32 Playback (MAX98357 DAC)   │
            │────────────────────────────────────────│
            │ 🎶  Stream & Play Audio Response       │
            └────────────────────────────────────────┘

```

---

## 📁 **Project Structure**

```bash
Auralens/
│
├── esp32/              # Firmware for Xiao ESP32-S3 Sense
│   ├── main.ino
│   └── config.h
│
├── server/             # Flask backend (Python)
│   ├── app.py
│   ├── asr/            # FasterWhisper STT module
│   ├── llm/            # Gemini API integration
│   ├── tts/            # Pyttsx3 TTS engine
│   └── utils/          # Helper utilities and preprocessing
│
└── README.md
└── README.md
```

---

## ⚙️ **Setup Instructions**

### 🧩 ESP32-S3 Setup

1. Open `esp32/main.ino` in Arduino IDE.
2. Select **Seeed XIAO ESP32-S3 Sense** board.
3. Add **WiFi credentials** and **Flask server IP**.
4. Connect **MAX98357 DAC** via I2S.
5. Upload firmware and restart device.

---

### 🖥️ Flask Backend Setup

```bash
# 1️⃣ Create and activate virtual environment
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# 2️⃣ Install dependencies
pip install -r requirements.txt

# 3️⃣ Run Flask server
python server/app.py
```

> 💡 _Use `ngrok` or `localtunnel` to make your Flask server accessible over the internet._

---

## 🔁 **Operational Flow**

| Step | Process              | Description                             |
| ---- | -------------------- | --------------------------------------- |
| 1️⃣   | Press Touch Button 1 | Record audio, send to backend           |
| 2️⃣   | Press Touch Button 2 | Capture image + record audio            |
| 3️⃣   | Flask Backend        | Transcribe → Reason → Generate response |
| 4️⃣   | Flask → ESP32        | Return `response.wav`                   |
| 5️⃣   | ESP32 → DAC          | Play audio response instantly           |

---

## 🧪 **Performance Benchmarks**

| Metric              | Description                         | Target                    |
| ------------------- | ----------------------------------- | ------------------------- |
| ⚡ Latency          | Audio → Response round trip         | ≤ 2s                      |
| 🗣️ ASR Accuracy     | FasterWhisper transcription quality | ≥ 95%                     |
| 🔊 Audio Quality    | Pyttsx3 + DAC clarity               | Natural, clear voice      |
| 🧠 Context Accuracy | Gemini multimodal reasoning         | High contextual relevance |

---

## 🚀 **Future Enhancements**

- [ ] 🧠 On-device LLM (offline mode)
- [ ] 🕵️ Real-time object recognition
- [ ] 📱 Companion mobile dashboard
- [ ] 🔋 Battery optimization for full-day use
- [ ] 🧩 Context memory for multi-turn dialogue

---

## 💡 **Design Philosophy**

> _"AI that empowers human senses — intuitive, private, and always near."_

Auralens stands on three design pillars:

- 🧩 **Modular Intelligence** — Separate, replaceable ASR, LLM, and TTS modules
- 🔐 **Privacy First** — Local SD storage and transient cloud inference
- ⚙️ **Edge Efficiency** — Ultra-light ESP32 implementation for wearables

---

## 🖼️ **Hardware Connections (Quick View)**

| Component       | Connection | Function               |
| --------------- | ---------- | ---------------------- |
| 🎤 MEMS Mic     | Built-in   | Captures voice         |
| 📷 Camera       | Built-in   | Captures image         |
| 🔊 MAX98357 DAC | I2S        | Plays response audio   |
| 🕹️ Touch Pin 1  | GPIO       | Audio-only mode        |
| 🕹️ Touch Pin 2  | GPIO       | Audio + Image mode     |
| 💾 SD Card      | SPI        | Temporary data storage |

---

## 📜 **License**

This project is licensed under the **MIT License** — open for learning, innovation, and contribution.

---

<div align="center">

### 🌌 _Inspiration_

> “Blending human perception with machine intelligence —
> Auralens redefines how we **see**, **hear**, and **interact** with the world.”

**Developed with ❤️ by [Ritish Mahajan](https://github.com/RITISHM)**

</div>
```
