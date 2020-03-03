/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "time.h"
#include <WiFiUdp.h>

// Replace with your network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_PASSWORD";

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto = false;

String lastPhoto = "";
String list = "";

// HTTP GET parameter
const char* PARAM_INPUT_1 = "photo";

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
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

// Stores the camera configuration parameters
camera_config_t config;

File root;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32-CAM SD Card Photo Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 25%; }
    .hori{ margin-bottom: 0%; }
    img { height:auto; width:100%; }
  </style>
</head>
<body>
  <div id="container">
    <h1>ESP32-CAM SD Card Photo Manager</h1>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
      <button onclick="window.open('/list','_blank')">VIEW AND DELETE PHOTOS</button>
    </p>
  </div>
  <img src="saved-photo" id="photo" alt="Image not found: failed to open image, image deleted or no microSD card inserted. Try refresh the page or restart your board.">
</body>
<script>
  var deg = 0;
  function capturePhoto(){
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function rotatePhoto(){
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{document.getElementById("container").className = "hori";}
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n){return Math.abs(n % 2)==1;}
</script>
</html>)rawliteral";

void setup() {
  // Turn-off the brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  Serial.println("Initializing the camera module...");
  configInitCamera();
  
  Serial.println("Initializing the MicroSD card module... ");
  initMicroSDCard();
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD_MMC, lastPhoto, "image/jpg", false);
  });
  
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", list.c_str());
  });
  
  server.on("/view", HTTP_GET, [](AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/view?photo=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(SD_MMC, inputMessage, "image/jpg", false);
  });
  
  // Send a GET request to <ESP_IP>/delete?photo=<inputMessage>
  server.on("/delete", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/delete?photo=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    deleteFile(SD_MMC, inputMessage.c_str());
    request->send(200, "text/html", "Done. Your photo named " + inputMessage + " was removed." +
                                     "<br><a href=\"/\">Return to Home Page</a> or <a href=\"/list\">view/delete other photos</a>.");
  });
  // Start server
  server.begin();
  
  root = SD_MMC.open("/");
  listDirectory(SD_MMC);
}

void loop() {
  if (takeNewPhoto) {
    takeSavePhoto();
    takeNewPhoto = false;
  }
  delay(1);
}

void configInitCamera(){
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
  config.pixel_format = PIXFORMAT_JPEG; //YUV422,GRAYSCALE,RGB565,JPEG

  // Select lower framesize if the camera doesn't support PSRAM
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10; //0-63 lower number means higher quality
    config.fb_count = 2;
  } 
  else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize the Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void initMicroSDCard(){
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
}

void takeSavePhoto(){
  struct tm timeinfo;
  char now[20];
  // Take Picture with Camera
  camera_fb_t  * fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  } 
  // Path where new picture will be saved in SD Card
  getLocalTime(&timeinfo);
  strftime(now, 20, "%Y%m%d_%H%M%S", &timeinfo); // Format Date & Time
  String path = "/photo_" + String(now) +".jpg";
  lastPhoto = path;
  Serial.printf("Picture file name: %s\n", path.c_str());
  // Save picture to microSD card
  fs::FS &fs = SD_MMC; 
  File file = fs.open(path.c_str(),FILE_WRITE);
  if(!file){
    Serial.printf("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf(" Saved: %s\n", path.c_str());
    listDirectory(SD_MMC);
  }
  file.close();
  esp_camera_fb_return(fb); 
}

void listDirectory(fs::FS &fs) {
  File root = fs.open("/");
  list = "";
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(!file.isDirectory()){
      String filename=String(file.name());
      filename.toLowerCase();
      if (filename.indexOf(".jpg")!=-1){
        list = "<tr><td><button onclick=\"window.open('/view?photo="+String(file.name())+"','_blank')\">View</button></td><td><button onclick=\"window.location.href='/delete?photo="+String(file.name())+"'\">Delete</button></td><td>"+String(file.name())+"</td><td></td></tr>"+list;
      }
    }
    lastPhoto = file.name();
    file = root.openNextFile();
  }
  
  if (list=="") {
    list="<tr>No photos Stored</tr>";
  }
  else {
    list="<h1>ESP32-CAM View and Delete Photos</h1><p><a href=\"/\">Return to Home Page</a></p><table><th colspan=\"2\">Actions</th><th>Filename</th>"+list+"</table>";
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){  
    Serial.println("File deleted");
    listDirectory(SD_MMC);
  } 
  else {
    Serial.println("Delete failed");
  }
}
