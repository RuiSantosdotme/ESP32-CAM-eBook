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
#include <LittleFS.h>
#include <FS.h>

// Replace with your network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto = false;
boolean takeNewPhoto2 = false;

// Photo File Name to save in LittleFS
String FILE_PHOTO = "/photo.jpg";
String FILE_PHOTO_2  = "/test-photo.jpg";

// CAMERA_MODEL_AI_THINKER
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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32-CAM Face Recognition</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <meta http-equiv="Access-Control-Allow-Headers" content="Origin, X-Requested-With, Content-Type, Accept">
    <meta http-equiv="Access-Control-Allow-Methods" content="GET,POST,PUT,DELETE,OPTIONS">
    <meta http-equiv="Access-Control-Allow-Origin" content="*">
    <title>ESP32-CAM Face Detection and Recognition</title>
    <script src="https:\/\/code.jquery.com/jquery-3.3.1.min.js"></script>
    <script src='https:\/\/cdn.jsdelivr.net/gh/justadudewhohacks/face-api.js@0.22.1/dist/face-api.min.js'></script>
    <style>
      #container { padding: 0; }
      canvas {
        position: absolute;
        top: 0;
        left: 0;
        z-index:999;
      }
      img {  
        width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
      .flex-container { display: flex; }
      @media only screen and (max-width: 600px) {
        .flex-container {
          flex-direction: column;
        }
      }
      button {
        padding: 10px;
        margin-bottom: 30px;
      }
    </style>
  </head>
  <body>
    <div class="flex-container">
      <div>
        <div id="container"></div>
        <img src="photo.jpg" id="newImage"></img>
        <h2>Face Detection and Recognition</h2>
        <button onclick="capturePhoto();">2# FACE RECOGNITION - CAPTURE PHOTO</button>
      </div>
      <div>
        <img src="test-photo.jpg"></img>
        <h2>Face Enrolled - Test Photo</h2>
        <button onclick="captureTestPhoto()">1# ENROLL NEW FACE - CAPTURE TEST PHOTO</button>
      </div>
    </div>
     <div>
        <p><b>How to use this ESP32-CAM Face Recognition example:</b></p>
        <ol>
          <li>Press the "ENROLL NEW FACE - CAPTURE TEST PHOTO" button and refresh the page</li>
          <li>Press the "FACE RECOGNITION - CAPTURE PHOTO" button</li>
          <li>Refresh the page. The face recognition code runs automatically, so you have to wait 5 to 40 seconds for an alert with the face recognition result.</li>
        </ol>
        <p><b>IMPORTANT:</b> if you don't see a popup with your Face Recognition result, refresh the web page and wait a few seconds. I only recommend testing this web server on a laptop/desktop computer with Google Chrome web browser.</p>
        <p>You can only do face recognition for one subject - your TEST PHOTO should only have one face.</p>
      </div>
  </body>
<script>
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture-photo", true);
    xhr.send();
  }
  function captureTestPhoto(){
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture-test-photo", true);
    xhr.send();
  }  
  var ShowImage = document.getElementById('newImage');
  const modelPath = 'https://ruisantosdotme.github.io/face-api.js/weights/';
  let currentStream;
  let displaySize = { width:320, height: 400 }
  let convas;
  let faceDetection;
  Promise.all([
    faceapi.nets.faceRecognitionNet.loadFromUri(modelPath),
    faceapi.nets.faceLandmark68Net.loadFromUri(modelPath),
    faceapi.nets.ssdMobilenetv1.loadFromUri(modelPath)
  ]).then(start());
  async function start() {
    const container = document.createElement('div')
    container.style.position = 'relative'
    document.body.append(container)
    const labeledFaceDescriptors = await loadLabeledImages()
    const faceMatcher = new faceapi.FaceMatcher(labeledFaceDescriptors, 0.6)
    if( document.getElementsByTagName("canvas").length == 0 ) {
      canvas = faceapi.createCanvasFromMedia(ShowImage)
      document.getElementById('container').append(canvas)
    }
    const displaySize = { width: ShowImage.width, height: ShowImage.height }
    faceapi.matchDimensions(canvas, displaySize)
    const detections = await faceapi.detectAllFaces(ShowImage).withFaceLandmarks().withFaceDescriptors()
    const resizedDetections = faceapi.resizeResults(detections, displaySize)
    const results = resizedDetections.map(d => faceMatcher.findBestMatch(d.descriptor))
    results.forEach((result, i) => {
      const box = resizedDetections[i].detection.box
      const drawBox = new faceapi.draw.DrawBox(box, { label: result.toString() })
      drawBox.draw(canvas)
      if( result.toString().indexOf("unknown") == -1) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', "/trigger", true);
        xhr.send();
      }
    })
    alert("DONE!");
  }
  function loadLabeledImages() {
    const labels = ['SARA SANTOS']
    return Promise.all(
      labels.map(async label => {
        const descriptions = []
        const img = await faceapi.fetchImage(window.location.href+"test-photo.jpg")
        const detections = await faceapi.detectSingleFace(img).withFaceLandmarks().withFaceDescriptor()
        descriptions.push(detections.descriptor)
        return new faceapi.LabeledFaceDescriptors(label, descriptions)
      })
    )
  }
</script>
</html>)rawliteral";


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }

  // Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", index_html);
  });

  server.on("/capture-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send(200, "text/plain", "Taking Photo");
  });
  
  server.on("/trigger", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Trigger action");
    request->send(200, "text/plain", "Trigger");
  });
  
  server.on("/capture-test-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto2 = true;
    request->send(200, "text/plain", "Taking Photo");
  });

  server.on("/photo.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, FILE_PHOTO.c_str(), "image/jpg", false);
  });
  
  server.on("/test-photo.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, FILE_PHOTO_2.c_str(), "image/jpg", false);
  });
  
  // Start server
  server.begin();
}

void loop() {
  if (takeNewPhoto) {
    takeNewPhoto = capturePhotoSaveLittleFS(FILE_PHOTO.c_str());
  }
  if (takeNewPhoto2) {
    takeNewPhoto2 = capturePhotoSaveLittleFS(FILE_PHOTO_2.c_str());
  }
  delay(1);
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs , const char* photoName) {
  File f_pic = fs.open( photoName );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to LittleFS
bool capturePhotoSaveLittleFS( const char* photoName ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    // Clean previous buffer
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb); // dispose the buffered image
    fb = NULL; // reset to capture errors
    // Get fresh image
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      ESP.restart();
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", photoName);
    File file = LittleFS.open(photoName, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(photoName);
      Serial.print(" - Size: ");
      Serial.print(fb->len);
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in LittleFS
    ok = checkPhoto(LittleFS, photoName);
  } while ( !ok );
  return false;
}
