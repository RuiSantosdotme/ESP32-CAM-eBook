/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Keep track of number of pictures
int pictureNumber = 0;

// Stores the camera configuration parameters
camera_config_t config;

// Button pin
const int buttonPin = 16;
int buttonState = HIGH;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  pinMode(buttonPin, INPUT);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
 
  Serial.begin(115200);
  
  // Initialize MicroSD
  Serial.print("Initializing the MicroSD card module... ");
  initMicroSDCard();

  // Initialize the camera
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Ok!");
}

void loop() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        Serial.println("Button Pressed");
        String path = "/picture" + String(pictureNumber) + ".jpg";  
        Serial.printf("Picture file name: %s\n", path.c_str());
        takeSavePhoto(path);
        pictureNumber++;
      }
    }
  }
  lastButtonState = reading;
}

void configInitCamera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_SVGA; // 800x600
  config.jpeg_quality = 10;           // Lower number = higher quality
  config.fb_count = 1;                // Single frame buffer
  config.fb_location = CAMERA_FB_IN_DRAM; // Force DRAM usage to avoid PSRAM 

  // Check PSRAM for debugging (optional)
  if (psramFound()) {
    Serial.println("PSRAM detected but not used.");
  } else {
    Serial.println("No PSRAM detected.");
  }

  // Initialize the Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
}

void initMicroSDCard() {
  Serial.println("Starting SD Card");
  // Use 1-bit mode to reduce memory and pin usage
  if (!SD_MMC.begin("/sdcard", true)) { // true = 1-bit mode
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    return;
  }
  Serial.println("SD Card initialized.");
}

void takeSavePhoto(String path) {
  camera_fb_t *fb = esp_camera_fb_get();  
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  fs::FS &fs = SD_MMC; 
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);
    Serial.printf("Saved file to path: %s\n", path.c_str());
  }
  file.close();

  esp_camera_fb_return(fb); 
}
