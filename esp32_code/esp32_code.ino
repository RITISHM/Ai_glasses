#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "ESP_I2S.h"
#include "FS.h"
#include "SD.h"

using namespace websockets;

// ========== CONFIGURATION ==========
const char* ssid = "Ritish-laptop";
const char* password = "07867860";
const char* ws_server = "ws://10.51.111.72:5000/upload";

// Recording settings
const int SAMPLE_RATE = 16000;
const int TOUCH_THRESHOLD = 40000;
const int CHUNK_SIZE = 4096;

// OPTIMIZATION: Increased upload chunk size for faster transfer
const int UPLOAD_CHUNK_SIZE = 32768;  // 32KB chunks (was 8KB)

// Connection settings - OPTIMIZED timeouts
const int MAX_RECONNECT_ATTEMPTS = 2;  // Reduced for faster failure
const int RECONNECT_DELAY_MS = 1000;   // Reduced delay
const int WS_TIMEOUT_MS = 45000;       // Reduced to 45s
const int UPLOAD_TIMEOUT_MS = 30000;   // Reduced to 30s

// I2S pins
const int I2S_MIC_SERIAL_CLOCK = 42;
const int I2S_MIC_LEFT_RIGHT_CLOCK = 41;
const int I2S_SPK_SERIAL_DATA = 3;
const int I2S_SPK_LEFT_RIGHT_CLOCK = 5;
const int I2S_SPK_SERIAL_CLOCK = 4;

I2SClass i2s;
WebsocketsClient wsClient;
bool uploadComplete = false;
bool systemReady = false;
File downloadFile;
size_t expectedDownloadSize = 0;
size_t downloadedBytes = 0;
bool receivingAudio = false;
unsigned long lastActivityTime = 0;

// OPTIMIZATION: Buffer for faster downloads
uint8_t* downloadBuffer = nullptr;
const size_t DOWNLOAD_BUFFER_SIZE = 32768;  // 32KB buffer

// ========== FORWARD DECLARATIONS ==========
void playAudioFile(const char* filename);
bool ensureWiFiConnected();
bool connectWebSocket();
void cleanupWebSocket();

// ========== CALLBACKS ==========
void onMessageCallback(WebsocketsMessage message) {
  lastActivityTime = millis();
  
  if (message.isText()) {
    String msg = message.data();
    
    // Check for error messages
    if (msg.indexOf("\"status\":\"error\"") >= 0) {
      Serial.println("❌ Server error");
      uploadComplete = true;
      return;
    }
    
    // Check if server is sending audio back
    if (msg.indexOf("\"audio_size\":") >= 0) {
      int sizeStart = msg.indexOf("\"audio_size\":") + 13;
      int sizeEnd = msg.indexOf(",", sizeStart);
      if (sizeEnd < 0) sizeEnd = msg.indexOf("}", sizeStart);
      
      String sizeStr = msg.substring(sizeStart, sizeEnd);
      expectedDownloadSize = sizeStr.toInt();
      
      Serial.printf("📥 Receiving response: %d KB\n", expectedDownloadSize/1024);
      
      // Close any existing file
      if (downloadFile) {
        downloadFile.close();
      }
      
      // Delete old response file if exists
      if (SD.exists("/response.wav")) {
        SD.remove("/response.wav");
      }
      
      // OPTIMIZATION: Allocate download buffer
      if (!downloadBuffer) {
        downloadBuffer = (uint8_t*)malloc(DOWNLOAD_BUFFER_SIZE);
      }
      
      // Open file for writing
      downloadFile = SD.open("/response.wav", FILE_WRITE);
      if (downloadFile) {
        receivingAudio = true;
        downloadedBytes = 0;
      } else {
        Serial.println("❌ SD write failed");
        uploadComplete = true;
      }
    } else if (msg.indexOf("\"sending_audio\":false") >= 0) {
      uploadComplete = true;
    }
  } 
  else if (message.isBinary() && receivingAudio) {
    size_t dataSize = message.length();
    
    if (downloadFile && dataSize > 0) {
      // OPTIMIZATION: Direct write without buffering
      size_t written = downloadFile.write((uint8_t*)message.c_str(), dataSize);
      downloadedBytes += written;
      
      // OPTIMIZATION: Batch flush every 64KB for better performance
      if (downloadedBytes % 65536 == 0) {
        downloadFile.flush();
      }
      
      // Check if download complete
      if (downloadedBytes >= expectedDownloadSize) {
        downloadFile.flush();
        downloadFile.close();
        receivingAudio = false;
        uploadComplete = true;
        
        Serial.println("✅ Download complete");
        
        // Verify file on SD card
        File verifyFile = SD.open("/response.wav", FILE_READ);
        if (verifyFile) {
          size_t actualSize = verifyFile.size();
          verifyFile.close();
          
          if (actualSize >= expectedDownloadSize) {
            Serial.print("size recieved=");
            Serial.println(actualSize/1024);
            Serial.println("▶️  Playing response...\n");
            delay(300);  // OPTIMIZATION: Reduced delay
            playAudioFile("/response.wav");
          }
        }
      }
    }
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionClosed) {
    if (receivingAudio && downloadFile) {
      downloadFile.flush();
      downloadFile.close();
      receivingAudio = false;
    }
    uploadComplete = true;
  }
  else if (event == WebsocketsEvent::GotPing || event == WebsocketsEvent::GotPong) {
    lastActivityTime = millis();
  }
}

// ========== WiFi MANAGEMENT ==========
bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  
  Serial.println("⚠️ WiFi reconnecting...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // Reduced attempts
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi OK");
    return true;
  }
  
  Serial.println("❌ WiFi failed");
  return false;
}

// ========== WebSocket MANAGEMENT ==========
bool connectWebSocket() {
  if (!ensureWiFiConnected()) {
    return false;
  }
  
  wsClient.onMessage(onMessageCallback);
  wsClient.onEvent(onEventsCallback);
  
  for (int attempt = 1; attempt <= MAX_RECONNECT_ATTEMPTS; attempt++) {
    Serial.printf("🔗 Connecting... (%d/%d)\n", attempt, MAX_RECONNECT_ATTEMPTS);
    
    if (wsClient.connect(ws_server)) {
      Serial.println("✅ Connected");
      lastActivityTime = millis();
      return true;
    }
    
    if (attempt < MAX_RECONNECT_ATTEMPTS) {
      delay(RECONNECT_DELAY_MS);
    }
  }
  
  return false;
}

void cleanupWebSocket() {
  if (wsClient.available()) {
    wsClient.close();
  }
  
  if (receivingAudio && downloadFile) {
    downloadFile.flush();
    downloadFile.close();
    receivingAudio = false;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(2000);  // Reduced delay
  
  Serial.println("\n🎤 Audio Recorder (OPTIMIZED)");
  Serial.println("==============================\n");

  // === Touch sensor ===
  touchAttachInterrupt(T1, [](){}, TOUCH_THRESHOLD);

  // === SD Card ===
  Serial.print("💾 SD Card...");
  if (!SD.begin(21)) {
    Serial.println(" ❌");
    while(1) delay(1000);
  }
  Serial.println(" ✅");
  
  if (SD.exists("/response.wav")) {
    SD.remove("/response.wav");
  }

  // === Microphone ===
  Serial.print("🎙️ Microphone...");
  i2s.setPinsPdmRx(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println(" ❌");
    while(1) delay(1000);
  }
  Serial.println(" ✅");

  // === WiFi ===
  Serial.print("📡 WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // Reduced attempts
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" ❌");
    while(1) delay(1000);
  }
  
  Serial.println(" ✅");
  Serial.printf("   %s (%d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  
  systemReady = true;
  Serial.println("\n✅ Ready! Touch T2 to record\n");
}

// ========== LOOP ==========
void loop() {
  if (!systemReady) {
    delay(1000);
    return;
  }
  
  // OPTIMIZATION: Less frequent WiFi checks
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {  // Check every 10s instead of 5s
    if (WiFi.status() != WL_CONNECTED) {
      ensureWiFiConnected();
    }
    lastWiFiCheck = millis();
  }
  
  int touchValue = touchRead(T2);
  
  if (touchValue > TOUCH_THRESHOLD) {
    Serial.println("🎙️ Recording...");
    
    unsigned long recordStart = millis();
    
    if (SD.exists("/recording.wav")) {
      SD.remove("/recording.wav");
    }
    
    File file = SD.open("/recording.wav", FILE_WRITE);
    
    if (!file) {
      Serial.println("❌ File open failed");
      delay(1000);
      return;
    }

    uint8_t wavHeader[44];
    writeWAVHeader(wavHeader, 0, SAMPLE_RATE);
    file.write(wavHeader, 44);
    
    size_t totalBytes = 0;
    uint8_t buffer[CHUNK_SIZE];
    
    while (touchRead(T2) > TOUCH_THRESHOLD) {
      size_t bytesRead = i2s.readBytes((char*)buffer, CHUNK_SIZE);
      if (bytesRead > 0) {
        file.write(buffer, bytesRead);
        totalBytes += bytesRead;
        
        // OPTIMIZATION: Batch flush every 32KB
        if (totalBytes % 32768 == 0) {
          file.flush();
        }
      }
      yield();
    }
    
    float recordDuration = (millis() - recordStart) / 1000.0;
    
    file.seek(0);
    writeWAVHeader(wavHeader, totalBytes, SAMPLE_RATE);
    file.write(wavHeader, 44);
    file.flush();
    file.close();
    
    Serial.printf("✅ Recorded: %.1fs, %d KB\n", recordDuration, totalBytes/1024);
    
    if (totalBytes > 1000) {
      uploadRecordingReliable(totalBytes + 44);
    } else {
      Serial.println("⚠️ Too short\n");
    }
    
    Serial.println("Ready\n");
    delay(500);
  }
  
  delay(50);
}

// ========== HELPER FUNCTIONS ==========
void writeWAVHeader(uint8_t* header, uint32_t dataSize, uint32_t sampleRate) {
  uint32_t fileSize = dataSize + 36;
  uint16_t bitsPerSample = 16;
  uint16_t numChannels = 1;
  uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  uint16_t blockAlign = numChannels * bitsPerSample / 8;
  
  memcpy(header + 0, "RIFF", 4);
  memcpy(header + 4, &fileSize, 4);
  memcpy(header + 8, "WAVE", 4);
  memcpy(header + 12, "fmt ", 4);
  uint32_t fmtSize = 16;
  memcpy(header + 16, &fmtSize, 4);
  uint16_t audioFormat = 1;
  memcpy(header + 20, &audioFormat, 2);
  memcpy(header + 22, &numChannels, 2);
  memcpy(header + 24, &sampleRate, 4);
  memcpy(header + 28, &byteRate, 4);
  memcpy(header + 32, &blockAlign, 2);
  memcpy(header + 34, &bitsPerSample, 2);
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &dataSize, 4);
}

// ========== OPTIMIZED UPLOAD WITH RELIABILITY ==========
void uploadRecordingReliable(size_t fileSize) {
  Serial.printf("📤 Uploading: %d KB\n", fileSize/1024);
  
  if (fileSize > 5000000) {
    Serial.println("❌ File too large");
    return;
  }
  
  uploadComplete = false;
  receivingAudio = false;
  downloadedBytes = 0;
  
  if (!connectWebSocket()) {
    Serial.println("❌ Connection failed");
    return;
  }
  
  delay(200);  // CRITICAL: Give server time to be ready
  
  String sizeMsg = String(fileSize);
  if (!wsClient.send(sizeMsg)) {
    Serial.println("❌ Send size failed");
    cleanupWebSocket();
    return;
  }
  
  Serial.println("✅ Size sent, waiting for server...");
  delay(100);  // Let server process size message

  File audioFile = SD.open("/recording.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("❌ File open failed");
    cleanupWebSocket();
    return;
  }

  // CRITICAL: Use smaller chunk size for reliability
  const int RELIABLE_CHUNK_SIZE = 4096;  // 4KB chunks work better
  uint8_t* buffer = (uint8_t*)malloc(RELIABLE_CHUNK_SIZE);
  
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    audioFile.close();
    cleanupWebSocket();
    return;
  }

  unsigned long sendStart = millis();
  size_t totalSent = 0;
  size_t bytesRead;
  int chunkCount = 0;
  int consecutiveFails = 0;
  const int MAX_CONSECUTIVE_FAILS = 5;
  
  Serial.println("📤 Sending chunks...");
  
  while ((bytesRead = audioFile.read(buffer, RELIABLE_CHUNK_SIZE)) > 0) {
    // Check WiFi every 10 chunks
    if (chunkCount % 10 == 0) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi lost during upload");
        if (!ensureWiFiConnected()) {
          Serial.println("❌ WiFi reconnect failed");
          break;
        }
        // Reconnect WebSocket too
        if (!wsClient.available()) {
          Serial.println("❌ WebSocket lost");
          break;
        }
      }
    }
    
    // Send chunk with retry
    bool sent = false;
    for (int retry = 0; retry < 3; retry++) {
      sent = wsClient.sendBinary((const char*)buffer, bytesRead);
      if (sent) break;
      delay(50);
    }
    
    if (!sent) {
      consecutiveFails++;
      Serial.printf("❌ Chunk %d failed (%d consecutive fails)\n", chunkCount, consecutiveFails);
      
      if (consecutiveFails >= MAX_CONSECUTIVE_FAILS) {
        Serial.println("❌ Too many consecutive failures");
        break;
      }
      delay(100);
      continue;
    }
    
    consecutiveFails = 0;
    totalSent += bytesRead;
    chunkCount++;
    
    // Progress indicator every 20KB
    if (totalSent % 20480 == 0) {
      Serial.printf("  %d KB / %d KB\n", totalSent/1024, fileSize/1024);
    }
    
    // CRITICAL: Poll WebSocket to handle incoming messages
    wsClient.poll();
    
    // Small delay for network stability
    delay(5);
    
    // Timeout check
    if (millis() - sendStart > UPLOAD_TIMEOUT_MS) {
      Serial.println("❌ Upload timeout");
      break;
    }
  }
  
  audioFile.close();
  
  if (totalSent == fileSize) {
    float uploadTime = (millis() - sendStart) / 1000.0;
    Serial.printf("✅ Upload complete: %d KB in %.1fs (%.1f KB/s)\n", 
                  totalSent/1024, uploadTime, (totalSent/1024)/uploadTime);
    
    // Send EOF marker
    delay(100);
    wsClient.send("EOF");
    Serial.println("✅ EOF sent");
    
    // Wait for server response
    Serial.print("⏳ Waiting for response");
    unsigned long waitStart = millis();
    lastActivityTime = millis();
    
    int dots = 0;
    while (!uploadComplete && (millis() - waitStart < WS_TIMEOUT_MS)) {
      wsClient.poll();
      
      // Progress dots
      if ((millis() - waitStart) % 1000 < 50 && dots < 30) {
        Serial.print(".");
        dots++;
      }
      
      // Activity timeout (no messages from server)
      if (millis() - lastActivityTime > 15000) {
        Serial.println("\n⚠️ Server not responding");
        break;
      }
      
      delay(50);
    }
    
    Serial.println();
    
    if (uploadComplete) {
      Serial.println("✅ Transaction complete\n");
    } else {
      Serial.println("⚠️ No response received\n");
    }
  } else {
    Serial.printf("❌ Upload incomplete: %d/%d KB (%.1f%%)\n", 
                  totalSent/1024, fileSize/1024, (totalSent*100.0)/fileSize);
  }

  free(buffer);
  cleanupWebSocket();
}

// ========== AUDIO PLAYBACK ==========
void playAudioFile(const char* filename) {
  Serial.println("🔊 Playing...");
  
  File audioFile = SD.open(filename, FILE_READ);
  if (!audioFile) {
    Serial.println("❌ File not found");
    return;
  }
  
  size_t fileSize = audioFile.size();
  
  if (fileSize < 44) {
    Serial.println("❌ Invalid file");
    audioFile.close();
    return;
  }
  
  audioFile.seek(44);
  size_t audioDataSize = fileSize - 44;
  
  i2s.end();
  delay(100);
  
  i2s.setPins(I2S_SPK_SERIAL_CLOCK, I2S_SPK_LEFT_RIGHT_CLOCK, I2S_SPK_SERIAL_DATA);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("❌ Speaker init failed");
    audioFile.close();
    return;
  }
  
  // OPTIMIZATION: Larger playback buffer
  const size_t PLAY_CHUNK_SIZE = 8192;  // Increased from 4096
  uint8_t* buffer = (uint8_t*)malloc(PLAY_CHUNK_SIZE);
  
  if (!buffer) {
    Serial.println("❌ Memory failed");
    audioFile.close();
    i2s.end();
    return;
  }
  
  size_t totalPlayed = 0;
  size_t bytesRead;
  
  while ((bytesRead = audioFile.read(buffer, PLAY_CHUNK_SIZE)) > 0) {
    i2s.write(buffer, bytesRead);
    totalPlayed += bytesRead;
  }
  
  Serial.println("✅ Playback done");
  
  free(buffer);
  audioFile.close();
  i2s.end();
  
  delay(100);
  i2s.setPinsPdmRx(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("❌ Mic reinit failed");
    systemReady = false;
  }
  
  Serial.println();
}