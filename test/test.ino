/*
  ESP32 SD I2S Music Player
  esp32-i2s-sd-player.ino
  Plays MP3 file from microSD card
  Uses MAX98357 I2S Amplifier Module
  Uses ESP32-audioI2S Library - https://github.com/schreibfaul1/ESP32-audioI2S
  * 
  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/

// Include required libraries
#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
 
// microSD Card Reader connections
#define SD_CS          21
#define SPI_MOSI      9
#define SPI_MISO      8
#define SPI_SCK       7
#define I2S_DOUT      1
#define I2S_BCLK      4
#define I2S_LRC       5

 // Create Audio object
Audio audio;
 
void setup() {
    
    // Set microSD Card CS as OUTPUT and set HIGH
    pinMode(SD_CS, OUTPUT);      
    digitalWrite(SD_CS, HIGH); 
    
    // Initialize SPI bus for microSD Card
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    
    // Start Serial Port
    Serial.begin(115200);
    
    // Start microSD Card
    if(!SD.begin(SD_CS))
    {
      Serial.println("Error accessing microSD card!");
      while(true); 
    }
    
    // Setup I2S 
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    
    // Set Volume
    audio.setVolume(20);
    
    // Open music file
    audio.connecttoFS(SD,"response.mp3");
    
}
 
void loop()
{
    audio.loop();  
    

  if (!audio.isRunning()) {
    Serial.println("Playback finished. Restarting...");
    audio.connecttoFS(SD, "response.mp3");  // Reconnect to restart playback
  }

}