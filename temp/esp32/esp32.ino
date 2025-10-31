#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "ESP_I2S.h"
#include "FS.h"
#include "SD.h"

using namespace websockets;

// ========== CONFIGURATION ==========
const char* ssid = "Ritish";
const char* password = "07867860";
const char* ws_server = "ws://10.229.202.72:5000/upload";

// Recording settings
const int RECORD_DURATION = 10;  // seconds
const int SAMPLE_RATE = 16000;   // Hz

I2SClass i2s;
WebsocketsClient wsClient;
bool uploadComplete = false;

// ========== CALLBACKS ==========
void onMessageCallback(WebsocketsMessage message) {
  uploadComplete = true;
}

void onEventsCallback(WebsocketsEvent event, String data) {
  // Silent event handling
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(2000);

  // === WiFi Connection ===
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // === SD Card Init ===
  if (!SD.begin(21)) {
    Serial.println("❌ SD init failed!");
    while(1) delay(1000);
  }

  // === I2S Init ===
  i2s.setPinsPdmRx(42, 41);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("❌ I2S init failed!");
    while(1) delay(1000);
  }

  // === Record Audio ===
  uint8_t* wav_buffer;
  size_t wav_size;
  wav_buffer = i2s.recordWAV(RECORD_DURATION, &wav_size);

  if (!wav_buffer || wav_size == 0) {
    Serial.println("❌ Recording failed!");
    while(1) delay(1000);
  }

  // === Save to SD Card ===
  File file = SD.open("/recorded.wav", FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to open file!");
    return;
  }
  file.write(wav_buffer, wav_size);
  file.close();

  // === Setup WebSocket ===
  wsClient.onMessage(onMessageCallback);
  wsClient.onEvent(onEventsCallback);

  // === Connect to WebSocket Server ===
  unsigned long wsConnectStart = millis();
  bool connected = wsClient.connect(ws_server);
  unsigned long wsConnectEnd = millis();
  
  if (!connected) {
    Serial.println("❌ WebSocket connection failed!");
    return;
  }
  
  delay(100);

  // === Read and Upload WAV File ===
  File audioFile = SD.open("/recorded.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("❌ Cannot open WAV file!");
    wsClient.close();
    return;
  }

  size_t fileSize = audioFile.size();
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  
  if (!buffer) {
    Serial.println("❌ Memory allocation failed!");
    audioFile.close();
    wsClient.close();
    return;
  }

  audioFile.read(buffer, fileSize);
  audioFile.close();

  // === Send via WebSocket ===
  unsigned long sendStart = millis();
  wsClient.sendBinary((const char*)buffer, fileSize);
  unsigned long sendEnd = millis();
  
  // === Wait for server response ===
  int timeout = 10000;
  unsigned long waitStart = millis();
  
  while (!uploadComplete && (millis() - waitStart < timeout)) {
    wsClient.poll();
    delay(10);
  }
  
  unsigned long waitEnd = millis();
  unsigned long totalEnd = millis();
  
  // === Display Timing ===
  if (uploadComplete) {
    float totalTime = (totalEnd - wsConnectStart) / 1000.0;
    float connectTime = (wsConnectEnd - wsConnectStart) / 1000.0;
    float sendTime = (sendEnd - sendStart) / 1000.0;
    float responseTime = (waitEnd - waitStart) / 1000.0;
    float uploadSpeed = (fileSize / 1024.0) / sendTime;
    
    Serial.println("\n⏱️ Timing Breakdown:");
    Serial.printf("  🔗 Connection: %.2f s\n", connectTime);
    Serial.printf("  📤 Transmission: %.2f s\n", sendTime);
    Serial.printf("  📨 Response: %.2f s\n", responseTime);
    Serial.printf("  ⏱️ Total: %.2f s\n", totalTime);
    Serial.printf("  🚀 Speed: %.2f KB/s\n", uploadSpeed);
  } else {
    Serial.println("❌ Upload timeout!");
  }

  // === Cleanup ===
  unsigned long closeStart = millis();
  free(buffer);
  wsClient.close();
  unsigned long closeEnd = millis();
  
  Serial.printf("  🔌 Close: %.2f s\n", (closeEnd - closeStart) / 1000.0);
}

// ========== LOOP ==========
void loop() {
  delay(1000);
}