#include "WiFi.h"
#include "HTTPClient.h"
#include "ESP_I2S.h"
#include "SD.h"
#include "SPI.h"
#include "Arduino.h"
#include "Audio.h"
#include "FS.h"
// ===========================================
// WiFi Configuration
// ===========================================
const char* ssid = "Ritish";
const char* password = "07867860";
const char* serverURL = "http://10.21.192.72:5000/audioresponse";

// ===========================================
// Audio Settings
// ===========================================
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 5

// ===========================================
// SD Card Configuration for XIAO ESP32S3
// ===========================================
#define SD_CS          21
#define SPI_MOSI       9
#define SPI_MISO       8
#define SPI_SCK        7
#define I2S_DOUT      1
#define I2S_BCLK      4
#define I2S_LRC       5
// Create an instance of the I2SClass
I2SClass i2s;
bool micReady = false;
bool sdReady = false;

Audio audio;
// Global buffer for audio data
uint8_t* audioBuffer = nullptr;
size_t audioBufferSize = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  delay(2000);
  
  Serial.println();
  Serial.println("=== XIAO ESP32S3 Audio Recorder with SD Card ===");
  Serial.println("Boot successful!");
  
  // System info
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("Free Heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  
  // Connect WiFi
  Serial.println("\nConnecting to WiFi...");
  connectWiFi();
  
  // Initialize internal PDM microphone
  Serial.println("\nInitializing internal PDM microphone...");
  setupInternalMicrophone();
  
  // Initialize SD Card
  Serial.println("\nInitializing SD Card...");
  setupSDCard();
   audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20);
  Serial.println("\n=== Setup Complete ===");
  Serial.println("Ready to record audio");
}

void loop() {
  Serial.println("\n--- Audio Recording ---");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
    delay(2000);
    return;
  }
  
  // Check SD Card
  if (!sdReady) {
    Serial.println("SD Card not ready, retrying...");
    setupSDCard();
    delay(2000);
  }
  
  // Check microphone
  if (!micReady) {
    Serial.println("Microphone not ready, retrying...");
    setupInternalMicrophone();
    delay(2000);
  }
  
  if (!micReady || !sdReady) {
    Serial.println("Waiting for all components to be ready...");
    delay(5000);
    return;
  }
  
  // Record audio from internal microphone
  Serial.println("Recording from internal PDM microphone...");
  if (recordFromInternalMic()) {
    Serial.println("✓ Audio recorded successfully to buffer");
    
    // Upload audio to server and get response
    if (uploadAudioWithResponse()) {
      Serial.println("✓ Upload successful and response saved!");
    } else {
      Serial.println("✗ Upload failed or no response received");
    }
  audio.connecttoFS(SD, "response.mp3");
  audio.loop();
  while(audio.isRunning());
    // Free audio buffer after upload
    if (audioBuffer) {
      free(audioBuffer);
      audioBuffer = nullptr;
      audioBufferSize = 0;
    }
  } else {
    Serial.println("✗ Audio recording failed");
  }
  
  Serial.println("Waiting 15 seconds before next recording...");
  delay(15000);
}

void connectWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.printf("✓ WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println();
    Serial.println("✗ WiFi connection failed!");
  }
}

void setupSDCard() {
  Serial.println("Configuring SD Card SPI...");
  
  // Initialize SPI with correct pins for XIAO ESP32S3
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  
  Serial.printf("SPI Pins - SCK:%d, MISO:%d, MOSI:%d, CS:%d\n", SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  
  // Try to mount SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("✗ SD Card initialization failed!");
    Serial.println("Please check:");
    Serial.println("  - SD card is inserted");
    Serial.println("  - SD card is formatted (FAT32)");
    Serial.println("  - Wiring is correct");
    sdReady = false;
    return;
  }
  
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD card detected!");
    sdReady = false;
    return;
  }
  
  Serial.print("✓ SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %llu MB\n", cardSize);
  Serial.printf("Total space: %llu MB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %llu MB\n", SD.usedBytes() / (1024 * 1024));
  
  sdReady = true;
  Serial.println("✓ SD Card ready");
}

void setupInternalMicrophone() {
  Serial.println("Initializing I2S bus...");
  
  // Set up PDM microphone pins (XIAO ESP32S3 internal mic)
  i2s.setPinsPdmRx(42, 41);
  
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ Failed to initialize I2S!");
    micReady = false;
    return;
  }
  
  micReady = true;
  Serial.println("✓ I2S bus initialized");
  Serial.printf("Sample rate: %d Hz\n", SAMPLE_RATE);
}

bool recordFromInternalMic() {
  Serial.printf("Recording %d seconds of audio data...\n", RECORD_SECONDS);
  
  // Free previous buffer if exists
  if (audioBuffer) {
    free(audioBuffer);
    audioBuffer = nullptr;
    audioBufferSize = 0;
  }
  
  // Record audio using the ESP_I2S library's recordWAV function
  // This automatically records in WAV format with proper headers
  audioBuffer = i2s.recordWAV(RECORD_SECONDS, &audioBufferSize);
  
  if (audioBuffer == nullptr || audioBufferSize == 0) {
    Serial.println("Failed to record audio data!");
    return false;
  }
  
  Serial.printf("Recorded %d bytes of WAV audio data in buffer\n", audioBufferSize);
  Serial.println("✓ Audio stored in memory buffer");
  
  // Verify WAV header (first 4 bytes should be "RIFF")
  if (audioBuffer[0] == 'R' && audioBuffer[1] == 'I' && audioBuffer[2] == 'F' && audioBuffer[3] == 'F') {
    Serial.println("✓ Valid WAV file header detected in buffer");
  } else {
    Serial.println("⚠ Warning: WAV header not detected, but proceeding anyway");
  }
  
  return true;
}

bool uploadAudioWithResponse() {
  // Check if audio buffer exists
  if (!audioBuffer || audioBufferSize == 0) {
    Serial.println("No audio data in buffer!");
    return false;
  }
  
  Serial.printf("Audio buffer size: %d bytes (%.2f KB)\n", audioBufferSize, audioBufferSize / 1024.0);
  
  // Create HTTP request
  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(60000);  // 30 second timeout to wait for server response
  
  String boundary = "XiaoInternalMicBoundary";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  // Build multipart form data (audio only)
  String audioHeader = "--" + boundary + "\r\n";
  audioHeader += "Content-Disposition: form-data; name=\"audio\"; filename=\"internal_mic.wav\"\r\n";
  audioHeader += "Content-Type: audio/wav\r\n\r\n";
  
  String footer = "\r\n--" + boundary + "--\r\n";
  
  Serial.printf("Uploading WAV audio: internal_mic.wav (%d bytes)\n", audioBufferSize);
  
  // Calculate payload size
  size_t totalSize = audioHeader.length() + audioBufferSize + footer.length();
  Serial.printf("Total upload size: %d bytes (%.2f KB)\n", totalSize, totalSize / 1024.0);
  
  // Create payload
  uint8_t* payload = (uint8_t*)malloc(totalSize);
  if (!payload) {
    Serial.println("✗ Failed to allocate upload payload");
    http.end();
    return false;
  }
  
  // Assemble payload
  size_t pos = 0;
  
  // Add audio header
  memcpy(payload + pos, audioHeader.c_str(), audioHeader.length());
  pos += audioHeader.length();
  
  // Add audio data from buffer
  memcpy(payload + pos, audioBuffer, audioBufferSize);
  pos += audioBufferSize;
  
  // Add footer
  memcpy(payload + pos, footer.c_str(), footer.length());
  
  Serial.println("Uploading audio to server...");
  Serial.println("Waiting up to 30 seconds for server response...");
  
  // Send POST request
  int httpCode = http.POST(payload, totalSize);
  
  // Free payload as we don't need it anymore
  free(payload);
  
  // Handle response
  bool success = false;
  if (httpCode > 0) {
    Serial.printf("HTTP Response: %d\n", httpCode);
    
    if (httpCode == 200) {
      Serial.println("✓ Server accepted the audio!");
      
      // Check if response contains audio data
      int contentLength = http.getSize();
      Serial.printf("Response content length: %d bytes\n", contentLength);
      
      if (contentLength > 0) {
        // Get response stream
        WiFiClient* stream = http.getStreamPtr();
        
        // Generate unique filename with timestamp
        String filename = "/response.mp3";
        
        Serial.printf("Saving server response to SD card: %s\n", filename.c_str());
        
        // Open file on SD card for writing
        File responseFile = SD.open(filename.c_str(), FILE_WRITE);
        if (!responseFile) {
          Serial.println("✗ Failed to create response file on SD card");
        } else {
          // Read response data and write to SD card
          uint8_t buffer[512];
          int totalBytes = 0;
          
          while (http.connected() && (contentLength > 0 || contentLength == -1)) {
            size_t availableBytes = stream->available();
            
            if (availableBytes) {
              int bytesToRead = ((availableBytes > sizeof(buffer)) ? sizeof(buffer) : availableBytes);
              int bytesRead = stream->readBytes(buffer, bytesToRead);
              
              if (bytesRead > 0) {
                responseFile.write(buffer, bytesRead);
                totalBytes += bytesRead;
                
                if (contentLength > 0) {
                  contentLength -= bytesRead;
                }
              }
            }
            delay(1);
          }
          
          responseFile.close();
          Serial.printf("✓ Response saved: %d bytes written to %s\n", totalBytes, filename.c_str());
          
          // Verify the file
          if (SD.exists(filename.c_str())) {
            File checkFile = SD.open(filename.c_str(), FILE_READ);
            Serial.printf("✓ File verified on SD card, size: %d bytes\n", checkFile.size());
            checkFile.close();
            success = true;
          } else {
            Serial.println("✗ File verification failed!");
          }
        }
      } else {
        Serial.println("⚠ Server returned empty response (no audio file)");
        success = true;  // Still consider upload successful
      }
    } else {
      Serial.println("✗ Server returned error");
      String response = http.getString();
      if (response.length() > 0 && response.length() < 400) {
        Serial.println("Server response:");
        Serial.println(response);
      }
    }
  } else {
    Serial.printf("✗ HTTP Error: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    Serial.println("Check server connection");
  }
  
  http.end();
 
  return success;
}