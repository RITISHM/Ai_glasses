#include "WiFi.h"
#include "HTTPClient.h"
#include "SPIFFS.h"
#include "ESP_I2S.h"
#include "esp_camera.h"

// ===========================================
// WiFi Configuration
// ===========================================
const char* ssid = "Ritish";
const char* password = "07867860";
const char* serverURL = "http://10.87.29.73:5000/upload";

// ===========================================
// Audio Settings
// ===========================================
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 3
#define AUDIO_FILE "/audio.wav"

// ===========================================
// Camera Configuration for XIAO ESP32S3 Sense with OV3660
// ===========================================
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// OV3660 supports up to 3.2MP (2048x1536) resolution

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

// Create an instance of the I2SClass
I2SClass i2s;
bool micReady = false;
bool cameraReady = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  delay(2000);
  
  Serial.println();
  Serial.println("=== XIAO ESP32S3 Sense Audio + Camera Test ===");
  Serial.println("Boot successful!");
  
  // System info
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("Free Heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  
  // Initialize SPIFFS
  Serial.println("\nInitializing SPIFFS...");
  if (SPIFFS.begin(true)) {
    Serial.println("✓ SPIFFS OK");
  } else {
    Serial.println("✗ SPIFFS failed");
    return;
  }
  
  // Connect WiFi
  Serial.println("\nConnecting to WiFi...");
  connectWiFi();
  
  // Initialize camera
  Serial.println("\nInitializing camera...");
  setupCamera();
  
  // Initialize internal PDM microphone using ESP_I2S library
  Serial.println("\nInitializing internal PDM microphone...");
  setupInternalMicrophone();
  
  Serial.println("\n=== Setup Complete ===");
  Serial.println("Ready to record audio and capture images");
}

void loop() {
  Serial.println("\n--- Audio Recording & Image Capture ---");
  
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
    delay(2000);
    return;
  }
  
  // Check camera and microphone
  if (!cameraReady) {
    Serial.println("Camera not ready, retrying...");
    setupCamera();
    delay(2000);
  }
  
  if (!micReady) {
    Serial.println("Microphone not ready, retrying...");
    setupInternalMicrophone();
    delay(2000);
  }
  
  if (!cameraReady || !micReady) {
    Serial.println("Waiting for camera and microphone to be ready...");
    delay(5000);
    return;
  }
  
  // Capture image first
  Serial.println("Capturing image from camera...");
  camera_fb_t* fb = captureImage();
  
  if (!fb) {
    Serial.println("✗ Image capture failed");
    delay(5000);
    return;
  }
  
  Serial.printf("✓ Image captured: %dx%d, %d bytes (%.2f MB)\n", 
                fb->width, fb->height, fb->len, fb->len / (1024.0 * 1024.0));
  
  // Record audio from internal microphone
  Serial.println("Recording from internal PDM microphone...");
  if (recordFromInternalMic()) {
    Serial.println("✓ Audio recorded successfully");
    
    // Upload both audio and real image to server
    if (uploadAudioAndImage(fb)) {
      Serial.println("✓ Upload successful!");
    } else {
      Serial.println("✗ Upload failed");
    }
  } else {
    Serial.println("✗ Audio recording failed");
  }
  
  // Free camera buffer
  esp_camera_fb_return(fb);
  
  Serial.println("Waiting 15 seconds before next recording...");
  delay(5000);
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

void setupCamera() {
  Serial.println("Configuring OV3660 camera for XIAO ESP32S3 Sense...");
  
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
  
  // OV3660 Full Resolution Configuration
  if (psramFound()) {
    Serial.println("PSRAM found - using OV3660 maximum resolution");
    config.frame_size = FRAMESIZE_QXGA;  // 2048x1536 (3.1MP) - Maximum for OV3660
    config.jpeg_quality = 6;             // Higher quality (0-63, lower = better)
    config.fb_count = 2;                 // Double buffering for smoother capture
    config.grab_mode = CAMERA_GRAB_LATEST; // Always get the latest frame
  } else {
    Serial.println("No PSRAM detected - using reduced resolution");
    config.frame_size = FRAMESIZE_UXGA;  // 1600x1200 (1.9MP) - Fallback
    config.jpeg_quality = 10;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("✗ Camera init failed with error 0x%x\n", err);
    cameraReady = false;
    return;
  }
  
  // Get camera sensor
  sensor_t* s = esp_camera_sensor_get();
  if (s == nullptr) {
    Serial.println("✗ Failed to get camera sensor");
    cameraReady = false;
    return;
  }
  
  // Check sensor ID for OV3660 detection
  // OV3660 typically has PID 0x3660
  Serial.printf("Camera sensor PID: 0x%04X\n", s->id.PID);
  
  // OV3660 specific optimizations - check by PID
  if (s->id.PID == 0x3660) {
    Serial.println("OV3660 detected by PID - applying optimal settings");
    
    // Optimal settings for OV3660 high resolution
    s->set_brightness(s, 0);     // -2 to 2 (0 = default)
    s->set_contrast(s, 1);       // -2 to 2 (slightly higher contrast)
    s->set_saturation(s, 0);     // -2 to 2 (natural colors)
    s->set_special_effect(s, 0); // 0 = No Effect
    s->set_whitebal(s, 1);       // Enable auto white balance
    s->set_awb_gain(s, 1);       // Enable AWB gain
    s->set_wb_mode(s, 0);        // 0 = Auto WB
    s->set_exposure_ctrl(s, 1);  // Enable auto exposure
    s->set_aec2(s, 0);           // Disable AEC sensor
    s->set_ae_level(s, 0);       // Auto exposure level
    s->set_aec_value(s, 400);    // Manual exposure value (0-1200)
    s->set_gain_ctrl(s, 1);      // Enable auto gain
    s->set_agc_gain(s, 0);       // Auto gain value
    s->set_gainceiling(s, (gainceiling_t)6); // Higher gain ceiling for OV3660
    s->set_bpc(s, 1);            // Enable black pixel correction
    s->set_wpc(s, 1);            // Enable white pixel correction
    s->set_raw_gma(s, 1);        // Enable gamma correction
    s->set_lenc(s, 1);           // Enable lens correction
    s->set_hmirror(s, 0);        // Disable horizontal mirror
    s->set_vflip(s, 0);          // Disable vertical flip
    s->set_dcw(s, 1);            // Enable downsize
    s->set_colorbar(s, 0);       // Disable color bar test pattern
    
    // Try to set maximum resolution
    if (s->set_framesize(s, FRAMESIZE_QXGA) == 0) {
      Serial.println("✓ Successfully set QXGA resolution (2048x1536)");
    } else if (s->set_framesize(s, FRAMESIZE_UXGA) == 0) {
      Serial.println("✓ Set UXGA resolution (1600x1200) - QXGA failed");
    } else {
      Serial.println("⚠ Using default resolution");
    }
    
  } else {
    Serial.printf("⚠ Camera sensor PID 0x%04X may not be OV3660, using generic settings\n", s->id.PID);
    // Generic settings for other sensors
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_lenc(s, 1);
  }
  
  cameraReady = true;
  Serial.println("✓ Camera initialized successfully");
}

camera_fb_t* captureImage() {
  if (!cameraReady) {
    Serial.println("Camera not ready");
    return nullptr;
  }
  
  // Take a photo
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("Camera capture failed");
    return nullptr;
  }
  
  // Check if image data is valid
  if (fb->len == 0) {
    Serial.println("Captured image has no data");
    esp_camera_fb_return(fb);
    return nullptr;
  }
  
  return fb;
}

void setupInternalMicrophone() {
  Serial.println("Initializing I2S bus...");
  
  // Set up the pins used for PDM audio input (XIAO ESP32S3 Sense internal mic)
  i2s.setPinsPdmRx(42, 41);
  
  // Start I2S at 16 kHz with 16-bits per sample, PDM RX mode, mono
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
  
  // Variables to store the audio data
  uint8_t* wav_buffer;
  size_t wav_size;
  
  // Record audio using the ESP_I2S library's recordWAV function
  // This automatically records in WAV format with proper headers
  wav_buffer = i2s.recordWAV(RECORD_SECONDS, &wav_size);
  
  if (wav_buffer == nullptr || wav_size == 0) {
    Serial.println("Failed to record audio data!");
    return false;
  }
  
  Serial.printf("Recorded %d bytes of WAV audio data\n", wav_size);
  Serial.println("✓ Audio automatically converted to WAV format by ESP_I2S library");
  
  // Verify WAV header (first 4 bytes should be "RIFF")
  if (wav_buffer[0] == 'R' && wav_buffer[1] == 'I' && wav_buffer[2] == 'F' && wav_buffer[3] == 'F') {
    Serial.println("✓ Valid WAV file header detected");
  } else {
    Serial.println("⚠ Warning: WAV header not detected, but proceeding anyway");
  }
  
  // Save the recorded WAV audio to SPIFFS
  File audioFile = SPIFFS.open(AUDIO_FILE, "w");
  if (!audioFile) {
    Serial.println("Failed to create audio file on SPIFFS");
    free(wav_buffer); // Free the allocated memory
    return false;
  }
  
  Serial.println("Writing WAV audio data to SPIFFS...");
  
  // Write the WAV audio data to the file
  if (audioFile.write(wav_buffer, wav_size) != wav_size) {
    Serial.println("Failed to write WAV audio data to file!");
    audioFile.close();
    free(wav_buffer);
    return false;
  }
  
  audioFile.close();
  free(wav_buffer); // Free the allocated memory
  
  Serial.println("✓ WAV audio data written to SPIFFS successfully");
  return true;
}

bool uploadAudioAndImage(camera_fb_t* fb) {
  // Read audio file from SPIFFS
  File audioFile = SPIFFS.open(AUDIO_FILE, "r");
  if (!audioFile) {
    Serial.println("Audio file not found on SPIFFS");
    return false;
  }
  
  size_t audioSize = audioFile.size();
  Serial.printf("Audio file size: %d bytes\n", audioSize);
  
  if (audioSize == 0) {
    Serial.println("Audio file is empty!");
    audioFile.close();
    return false;
  }
  
  uint8_t* audioData = (uint8_t*)malloc(audioSize);
  if (!audioData) {
    Serial.println("Failed to allocate memory for audio");
    audioFile.close();
    return false;
  }
  
  audioFile.read(audioData, audioSize);
  audioFile.close();
  
  // Use real camera image data
  uint8_t* imageData = fb->buf;
  size_t imageSize = fb->len;
  
  Serial.printf("Real image size: %d bytes (%.2f MB)\n", imageSize, imageSize / (1024.0 * 1024.0));
  
  // Create HTTP request
  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(30000);  // 30 second timeout for larger files
  
  String boundary = "XiaoInternalMicCameraBoundary";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  // Build multipart form data
  String audioHeader = "--" + boundary + "\r\n";
  audioHeader += "Content-Disposition: form-data; name=\"audio\"; filename=\"internal_mic.wav\"\r\n";
  audioHeader += "Content-Type: audio/wav\r\n\r\n";
  
  String imageHeader = "\r\n--" + boundary + "\r\n";
  imageHeader += "Content-Disposition: form-data; name=\"image\"; filename=\"camera_capture.jpg\"\r\n";
  imageHeader += "Content-Type: image/jpeg\r\n\r\n";
  
  String footer = "\r\n--" + boundary + "--\r\n";
  
  Serial.printf("Uploading WAV audio file: internal_mic.wav (%d bytes)\n", audioSize);
  
  // Calculate payload size
  size_t totalSize = audioHeader.length() + audioSize + imageHeader.length() + imageSize + footer.length();
  Serial.printf("Upload payload size: %d bytes (%.2f MB)\n", totalSize, totalSize / (1024.0 * 1024.0));
  
  // Create payload
  uint8_t* payload = (uint8_t*)malloc(totalSize);
  if (!payload) {
    Serial.println("Failed to allocate upload payload");
    free(audioData);
    http.end();
    return false;
  }
  
  // Assemble payload
  size_t pos = 0;
  
  // Add audio header
  memcpy(payload + pos, audioHeader.c_str(), audioHeader.length());
  pos += audioHeader.length();
  
  // Add audio data
  memcpy(payload + pos, audioData, audioSize);
  pos += audioSize;
  
  // Add image header
  memcpy(payload + pos, imageHeader.c_str(), imageHeader.length());
  pos += imageHeader.length();
  
  // Add real image data
  memcpy(payload + pos, imageData, imageSize);
  pos += imageSize;
  
  // Add footer
  memcpy(payload + pos, footer.c_str(), footer.length());
  
  Serial.println("Uploading audio and real camera image to server...");
  
  // Send POST request
  int httpCode = http.POST(payload, totalSize);
  
  // Handle response
  bool success = false;
  if (httpCode > 0) {
    Serial.printf("HTTP Response: %d\n", httpCode);
    
    if (httpCode == 200) {
      Serial.println("✓ Server accepted the audio and image!");
      success = true;
    } else {
      Serial.println("Server returned error");
    }
    
    // Show server response
    String response = http.getString();
    if (response.length() > 0 && response.length() < 400) {
      Serial.println("Server response:");
      Serial.println(response);
    }
  } else {
    Serial.printf("HTTP Error: %d\n", httpCode);
    Serial.println("Check server connection");
  }
  
  // Cleanup
  free(audioData);
  free(payload);
  http.end();
  
  return success;
}