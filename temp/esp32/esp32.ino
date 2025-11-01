#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "ESP_I2S.h"
#include "FS.h"
#include "SD.h"

using namespace websockets;

// ========== CONFIGURATION ==========
const char* ssid = "Ritish";
const char* password = "07867860";
const char* ws_server = "ws://10.143.92.72:5000/upload";

// Recording settings
const int SAMPLE_RATE = 16000;
const int TOUCH_THRESHOLD = 40000;
const int CHUNK_SIZE = 4096;
const int UPLOAD_CHUNK_SIZE = 4096;  // Smaller chunks are more reliable

I2SClass i2s;
WebsocketsClient wsClient;
bool uploadComplete = false;
bool systemReady = false;

// ========== CALLBACKS ==========
void onMessageCallback(WebsocketsMessage message) {
  Serial.printf("📨 Server: %s\n", message.data().c_str());
  uploadComplete = true;
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("⚠️ Connection closed");
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n\n🎤 Touch-Controlled Audio Recorder (RELIABLE FAST)");
  Serial.println("==================================================\n");

  // === Configure Touch Pin ===
  Serial.println("⚙️ Configuring touch sensor...");
  touchAttachInterrupt(T1, [](){}, TOUCH_THRESHOLD);
  Serial.printf("   Touch threshold: %d\n", TOUCH_THRESHOLD);
  Serial.printf("   Current value: %d\n\n", touchRead(T1));

  // === SD Card Init ===
  Serial.print("💾 Initializing SD Card...");
  if (!SD.begin(21)) {
    Serial.println(" ❌ FAILED!");
    while(1) delay(1000);
  }
  Serial.println(" ✅");

  // === I2S Init ===
  Serial.print("🎙️ Initializing microphone...");
  i2s.setPinsPdmRx(42, 41);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println(" ❌ FAILED!");
    while(1) delay(1000);
  }
  Serial.println(" ✅");

  // === WiFi Connection ===
  Serial.print("📡 Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi connection failed!");
    while(1) delay(1000);
  }
  
  Serial.println(" ✅");
  Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("\n💾 Free heap: %d bytes\n", ESP.getFreeHeap());
  
  systemReady = true;
  Serial.println("\n✅ System ready! Touch T1 to record...\n");
}

// ========== LOOP ==========
void loop() {
  if (!systemReady) {
    delay(1000);
    return;
  }
  
  int touchValue = touchRead(T1);
  
  if (touchValue < TOUCH_THRESHOLD) {
    Serial.println("👆 Touch detected! Recording...");
    
    unsigned long recordStart = millis();
    File file = SD.open("/recording.wav", FILE_WRITE);
    
    if (!file) {
      Serial.println("❌ Failed to open file!");
      delay(1000);
      return;
    }

    uint8_t wavHeader[44];
    writeWAVHeader(wavHeader, 0, SAMPLE_RATE);
    file.write(wavHeader, 44);
    
    size_t totalBytes = 0;
    uint8_t buffer[CHUNK_SIZE];
    int dotCount = 0;
    
    while (touchRead(T1) < TOUCH_THRESHOLD) {
      size_t bytesRead = i2s.readBytes((char*)buffer, CHUNK_SIZE);
      if (bytesRead > 0) {
        file.write(buffer, bytesRead);
        totalBytes += bytesRead;
        if (++dotCount % 10 == 0) Serial.print(".");
      }
      yield();
    }
    
    Serial.println();
    float recordDuration = (millis() - recordStart) / 1000.0;
    
    file.seek(0);
    writeWAVHeader(wavHeader, totalBytes, SAMPLE_RATE);
    file.write(wavHeader, 44);
    file.close();
    
    Serial.printf("🎵 Recording complete: %.2f seconds, %d bytes\n", recordDuration, totalBytes);
    
    if (totalBytes > 1000) {
      uploadRecordingReliable(totalBytes + 44);
    } else {
      Serial.println("⚠️ Recording too short, skipping upload\n");
    }
    
    Serial.println("✅ Ready. Touch T1 to record...\n");
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

// ========== FAST CHUNKED UPLOAD WITH ACK ==========
void uploadRecordingReliable(size_t fileSize) {
  Serial.println("📤 Starting upload...");
  Serial.printf("📊 File size: %d bytes (%.2f KB)\n", fileSize, fileSize/1024.0);
  
  if (fileSize > 5000000) {
    Serial.println("❌ File too large!");
    return;
  }
  
  wsClient.onMessage(onMessageCallback);
  wsClient.onEvent(onEventsCallback);
  uploadComplete = false;

  Serial.print("🔗 Connecting...");
  unsigned long wsConnectStart = millis();
  
  if (!wsClient.connect(ws_server)) {
    Serial.println(" ❌ Failed!");
    return;
  }
  Serial.println(" ✅");
  unsigned long wsConnectEnd = millis();
  
  delay(100);  // Let connection stabilize
  
  // Send file size first as text message
  String sizeMsg = String(fileSize);
  wsClient.send(sizeMsg);
  Serial.printf("📏 Sent size: %d bytes\n", fileSize);
  delay(50);

  // Open file
  File audioFile = SD.open("/recording.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("❌ Cannot open file!");
    wsClient.close();
    return;
  }

  // Allocate buffer
  uint8_t* buffer = (uint8_t*)malloc(UPLOAD_CHUNK_SIZE);
  if (!buffer) {
    Serial.println("❌ Memory allocation failed!");
    audioFile.close();
    wsClient.close();
    return;
  }

  Serial.println("📤 Uploading chunks...");
  unsigned long sendStart = millis();
  
  size_t totalSent = 0;
  size_t bytesRead;
  int chunkCount = 0;
  int progressCount = 0;
  
  // Stream file in chunks
  while ((bytesRead = audioFile.read(buffer, UPLOAD_CHUNK_SIZE)) > 0) {
    // Send chunk
    bool sent = wsClient.sendBinary((const char*)buffer, bytesRead);
    
    if (!sent) {
      Serial.println("\n❌ Send failed!");
      break;
    }
    
    totalSent += bytesRead;
    chunkCount++;
    
    // Progress indicator every 10 chunks
    if (++progressCount >= 10) {
      Serial.printf("  📦 %d KB / %d KB\n", totalSent/1024, fileSize/1024);
      progressCount = 0;
    }
    
    // Tiny delay to prevent buffer overflow
    delayMicroseconds(50);
    
    // Keep connection alive
    wsClient.poll();
  }
  
  audioFile.close();
  unsigned long sendEnd = millis();
  
  Serial.printf("✅ Sent %d bytes in %d chunks\n", totalSent, chunkCount);

  // Send completion marker
  wsClient.send("EOF");
  Serial.println("📨 Sent EOF marker");

  // Wait for server response
  Serial.print("⏳ Waiting for response...");
  unsigned long waitStart = millis();
  int timeout = 10000;
  
  while (!uploadComplete && (millis() - waitStart < timeout)) {
    wsClient.poll();
    delay(10);
  }
  
  unsigned long waitEnd = millis();
  unsigned long totalEnd = millis();
  
  if (uploadComplete) {
    Serial.println(" ✅");
    Serial.println("\n⏱️ Timing Breakdown:");
    float connectTime = (wsConnectEnd - wsConnectStart) / 1000.0;
    float sendTime = (sendEnd - sendStart) / 1000.0;
    float responseTime = (waitEnd - waitStart) / 1000.0;
    float totalTime = (totalEnd - wsConnectStart) / 1000.0;
    float uploadSpeed = (totalSent / 1024.0) / sendTime;
    
    Serial.printf("  🔗 Connection: %.3f s\n", connectTime);
    Serial.printf("  📤 Upload: %.3f s\n", sendTime);
    Serial.printf("  📨 Response: %.3f s\n", responseTime);
    Serial.printf("  ⏱️ Total: %.3f s\n", totalTime);
    Serial.printf("  🚀 SPEED: %.2f KB/s\n", uploadSpeed);
    Serial.printf("  📊 Sent: %d bytes (%d chunks)\n", totalSent, chunkCount);
  } else {
    Serial.println(" ❌ Timeout!");
  }

  free(buffer);
  wsClient.close();
  Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());
}