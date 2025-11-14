#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "ESP_I2S.h"
#include "FS.h"
#include "SD.h"
#include "esp_camera.h"

using namespace websockets;

// ========== CONFIGURATION ==========
const char* ssid = "Ritish-laptop";
const char* password = "07867860";
const char* ws_server = "ws://10.114.127.72:5000/upload";

// Recording settings
const int SAMPLE_RATE = 16000;
const int TOUCH_THRESHOLD = 40000;
const int CHUNK_SIZE = 4096;

// OPTIMIZATION: Increased upload chunk size for faster transfer
const int UPLOAD_CHUNK_SIZE = 32768;  // 32KB chunks (was 8KB)

// Connection settings - OPTIMIZED timeouts
const int MAX_RECONNECT_ATTEMPTS = 2;  // Reduced for faster failure
const int RECONNECT_DELAY_MS = 1000;   // Reduced delay
const int WS_TIMEOUT_MS = 60000;       // Reduced to 45s
const int UPLOAD_TIMEOUT_MS = 60000;   // Reduced to 30s

// I2S pins
const int I2S_MIC_SERIAL_CLOCK = 42;
const int I2S_MIC_LEFT_RIGHT_CLOCK = 41;
const int I2S_SPK_SERIAL_DATA = 5;
const int I2S_SPK_LEFT_RIGHT_CLOCK = 3;
const int I2S_SPK_SERIAL_CLOCK = 4;

// Camera pins for OV2640 (adjust based on your wiring)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

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
bool initCamera();
bool captureAndSaveImage(const char* filename);

// ========== CAMERA INITIALIZATION ==========
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Image quality settings
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;  
    config.jpeg_quality =10;  // 0-63, lower means higher quality
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;
  }
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  
  return true;
}

// ========== CAPTURE AND SAVE IMAGE ==========
bool captureAndSaveImage(const char* filename) {
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Camera capture failed");
    return false;
  }
  
  Serial.printf("📸 Captured: %d KB (%dx%d)\n", fb->len/1024, fb->width, fb->height);
  
  // Save to SD card
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to open image file");
    esp_camera_fb_return(fb);
    return false;
  }
  
  file.write(fb->buf, fb->len);
  file.close();
  
  esp_camera_fb_return(fb);
  
  Serial.printf("💾 Image saved: %s\n", filename);
  return true;
}

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
            Serial.print("size received=");
            Serial.println(actualSize/1024);
            Serial.println("▶️  Playing response...\n");
            delay(300);
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
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
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
  delay(2000);
  
  Serial.println("\n🎤📸 Audio & Image Recorder (OPTIMIZED)");
  Serial.println("========================================\n");

  // === Touch sensor ===
  // touchAttachInterrupt(T1, [](){}, TOUCH_THRESHOLD);

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

  // === Camera ===
  Serial.print("📸 Camera...");
  if (!initCamera()) {
    Serial.println(" ❌");
    while(1) delay(1000);
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
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
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
  Serial.println("\n✅ Ready! Touch T2 to capture image & record\n");
}

// ========== LOOP ==========
void loop() {
  if (!systemReady) {
    delay(1000);
    return;
  }
  for (int i = 0; i < 3; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
      delay(100);  // Small delay between flushes
    }
  }
  // OPTIMIZATION: Less frequent WiFi checks
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      ensureWiFiConnected();
    }
    lastWiFiCheck = millis();
  }
  
  int touchValue = touchRead(T2);
  
  if (touchValue > TOUCH_THRESHOLD) {
    unsigned long sessionStart = millis();
    
    // ===== STEP 1: CAPTURE IMAGE =====
    Serial.println("📸 Capturing image...");
    
    char imageFilename[32];
    sprintf(imageFilename, "/image_%lu.jpg", millis());
    
    if (!captureAndSaveImage(imageFilename)) {
      Serial.println("⚠️ Image capture failed, continuing with audio only\n");
    }
    
    delay(200);  // Small delay between capture and recording
    
    // ===== STEP 2: RECORD AUDIO =====
    Serial.println("🎙️ Recording audio...");
    
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
    
    // ===== STEP 3: UPLOAD BOTH FILES =====
    if (totalBytes > 1000) {
      uploadImageAndAudio(imageFilename, "/recording.wav", totalBytes + 44);
    } else {
      Serial.println("⚠️ Audio too short\n");
    }
    
    float sessionDuration = (millis() - sessionStart) / 1000.0;
    Serial.printf("✅ Session complete (%.1fs)\n", sessionDuration);
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

// ========== UPLOAD IMAGE AND AUDIO ==========
void uploadImageAndAudio(const char* imageFile, const char* audioFile, size_t audioSize) {
  Serial.println("\n📤 Uploading image and audio...");
  
  uploadComplete = false;
  receivingAudio = false;
  downloadedBytes = 0;
  
  if (!connectWebSocket()) {
    Serial.println("❌ Connection failed");
    return;
  }
  
  delay(200);
  
  // Get image size
  size_t imageSize = 0;
  bool hasImage = SD.exists(imageFile);
  
  if (hasImage) {
    File imgFile = SD.open(imageFile, FILE_READ);
    if (imgFile) {
      imageSize = imgFile.size();
      imgFile.close();
    }
  }
  
  // Send metadata: image_size,audio_size
  String metadata = String(imageSize) + "," + String(audioSize);
  if (!wsClient.send(metadata)) {
    Serial.println("❌ Metadata send failed");
    cleanupWebSocket();
    return;
  }
  
  Serial.printf("✅ Metadata sent: Image=%d KB, Audio=%d KB\n", imageSize/1024, audioSize/1024);
  delay(100);
  
  // Upload image first (if exists)
  if (hasImage && imageSize > 0) {
    Serial.println("📤 Uploading image...");
    if (!uploadFile(imageFile, imageSize)) {
      Serial.println("⚠️ Image upload failed, continuing with audio");
    }
  }
  
  // Upload audio
  Serial.println("📤 Uploading audio...");
  if (!uploadFile(audioFile, audioSize)) {
    Serial.println("❌ Audio upload failed");
    cleanupWebSocket();
    return;
  }
  
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
    
    if ((millis() - waitStart) % 1000 < 50 && dots < 30) {
      Serial.print(".");
      dots++;
    }
    
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
  
  cleanupWebSocket();
}

// ========== UPLOAD SINGLE FILE ==========
bool uploadFile(const char* filename, size_t fileSize) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.printf("❌ Failed to open %s\n", filename);
    return false;
  }
  
  const int RELIABLE_CHUNK_SIZE = 4096;
  uint8_t* buffer = (uint8_t*)malloc(RELIABLE_CHUNK_SIZE);
  
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    file.close();
    return false;
  }
  
  size_t totalSent = 0;
  size_t bytesRead;
  int consecutiveFails = 0;
  const int MAX_CONSECUTIVE_FAILS = 5;
  
  while ((bytesRead = file.read(buffer, RELIABLE_CHUNK_SIZE)) > 0) {
    bool sent = false;
    for (int retry = 0; retry < 3; retry++) {
      sent = wsClient.sendBinary((const char*)buffer, bytesRead);
      if (sent) break;
      delay(50);
    }
    
    if (!sent) {
      consecutiveFails++;
      if (consecutiveFails >= MAX_CONSECUTIVE_FAILS) {
        Serial.println("❌ Too many failures");
        free(buffer);
        file.close();
        return false;
      }
      delay(100);
      continue;
    }
    
    consecutiveFails = 0;
    totalSent += bytesRead;
    
    if (totalSent % 20480 == 0) {
      Serial.printf("  %d KB / %d KB\n", totalSent/1024, fileSize/1024);
    }
    
    wsClient.poll();
    delay(5);
  }
  
  file.close();
  free(buffer);
  
  if (totalSent == fileSize) {
    Serial.printf("✅ %s uploaded (%d KB)\n", filename, totalSent/1024);
    return true;
  } else {
    Serial.printf("❌ Incomplete: %d/%d KB\n", totalSent/1024, fileSize/1024);
    return false;
  }
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
  
  i2s.end();
  delay(100);
  
  i2s.setPins(I2S_SPK_SERIAL_CLOCK, I2S_SPK_LEFT_RIGHT_CLOCK, I2S_SPK_SERIAL_DATA);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("❌ Speaker init failed");
    audioFile.close();
    return;
  }
  
  const size_t PLAY_CHUNK_SIZE = 8192;
  uint8_t* buffer = (uint8_t*)malloc(PLAY_CHUNK_SIZE);
  
  if (!buffer) {
    Serial.println("❌ Memory failed");
    audioFile.close();
    i2s.end();
    return;
  }
  
  size_t bytesRead;
  
  while ((bytesRead = audioFile.read(buffer, PLAY_CHUNK_SIZE)) > 0) {
    i2s.write(buffer, bytesRead);
  }
  
  Serial.println("✅ Playback done");
  for (int i = 0; i < 3; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
      delay(100);  // Small delay between flushes
    }
  }
  Serial.println(" ✅");
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