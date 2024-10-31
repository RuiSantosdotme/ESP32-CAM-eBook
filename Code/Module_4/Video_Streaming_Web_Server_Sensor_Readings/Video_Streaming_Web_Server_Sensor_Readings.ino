/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "Arduino.h"
#include "esp_camera.h"
#include "ESPAsyncWebServer.h"
#include <WiFi.h>
#include <Wire.h>
#include "SparkFunBME280.h"

// Replace with your network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// Define I2C Pins for BME280
#define I2C_SDA 14
#define I2C_SCL 15
BME280 bme;

// ESP32 AI Thinker Module Pinout
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

typedef struct {
  camera_fb_t * fb;
  size_t index;
} camera_frame_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";
  static const char * JPG_CONTENT_TYPE = "image/jpeg";
static const char * BMP_CONTENT_TYPE = "image/x-windows-bmp";

class AsyncJpegStreamResponse: public AsyncAbstractResponse {
  private:
    camera_frame_t _frame;
    size_t _index;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    uint64_t lastAsyncRequest;
  public:
  AsyncJpegStreamResponse(){
    _callback = nullptr;
    _code = 200;
    _contentLength = 0;
    _contentType = STREAM_CONTENT_TYPE;
    _sendContentLength = false;
    _chunked = true;
    _index = 0;
    _jpg_buf_len = 0;
    _jpg_buf = NULL;
    lastAsyncRequest = 0;
    memset(&_frame, 0, sizeof(camera_frame_t));
  }
  ~AsyncJpegStreamResponse(){
    if(_frame.fb){
      if(_frame.fb->format != PIXFORMAT_JPEG){
        free(_jpg_buf);
      }
      esp_camera_fb_return(_frame.fb);
    }
  }
  bool _sourceValid() const {
    return true;
  }
  virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
    size_t ret = _content(buf, maxLen, _index);
    if(ret != RESPONSE_TRY_AGAIN){
        _index += ret;
    }
    return ret;
  }
  size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
    if(!_frame.fb || _frame.index == _jpg_buf_len){
      if(index && _frame.fb){
        uint64_t end = (uint64_t)micros();
        int fp = (end - lastAsyncRequest) / 1000;
        log_printf("Size: %uKB, Time: %ums (%.1ffps)\n", _jpg_buf_len/1024, fp);
        lastAsyncRequest = end;
        if(_frame.fb->format != PIXFORMAT_JPEG){
          free(_jpg_buf);
        }
        esp_camera_fb_return(_frame.fb);
        _frame.fb = NULL;
        _jpg_buf_len = 0;
        _jpg_buf = NULL;
      }
      if(maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) + strlen(JPG_CONTENT_TYPE) + 8)){
        //log_w("Not enough space for headers");
        return RESPONSE_TRY_AGAIN;
      }
      //get frame
      _frame.index = 0;

      _frame.fb = esp_camera_fb_get();
      if (_frame.fb == NULL) {
        log_e("Camera frame failed");
        return 0;
      }

      if(_frame.fb->format != PIXFORMAT_JPEG){
        unsigned long st = millis();
        bool jpeg_converted = frame2jpg(_frame.fb, 80, &_jpg_buf, &_jpg_buf_len);
        if(!jpeg_converted){
          log_e("JPEG compression failed");
          esp_camera_fb_return(_frame.fb);
          _frame.fb = NULL;
          _jpg_buf_len = 0;
          _jpg_buf = NULL;
          return 0;
        }
        log_i("JPEG: %lums, %uB", millis() - st, _jpg_buf_len);
      } else {
        _jpg_buf_len = _frame.fb->len;
        _jpg_buf = _frame.fb->buf;
      }

      //send boundary
      size_t blen = 0;
      if(index){
        blen = strlen(STREAM_BOUNDARY);
        memcpy(buffer, STREAM_BOUNDARY, blen);
        buffer += blen;
      }
      //send header
      size_t hlen = sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, _jpg_buf_len);
      buffer += hlen;
      //send frame
      hlen = maxLen - hlen - blen;
      if(hlen > _jpg_buf_len){
        maxLen -= hlen - _jpg_buf_len;
        hlen = _jpg_buf_len;
      }
      memcpy(buffer, _jpg_buf, hlen);
      _frame.index += hlen;
      return maxLen;
    }
  
    size_t available = _jpg_buf_len - _frame.index;
    if(maxLen > available){
      maxLen = available;
    }
    memcpy(buffer, _jpg_buf+_frame.index, maxLen);
    _frame.index += maxLen;
  
    return maxLen;
  }
};

void streamJpg(AsyncWebServerRequest *request){
  AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
  if(!response){
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

float temperature;
float humidity;
float pressure;

// Get BME280 sensor readings and return them as a String variable
String getReadings(){
  temperature = bme.readTempC();
  //temperature = bme.readTempF();
  humidity = bme.readFloatHumidity();
  pressure = bme.readFloatPressure() / 100.0F;
  String message = "Temperature: " + String(temperature) + " ºC \n";
  message += "Humidity: " + String (humidity) + " % \n";
  message += "Pressure: " + String (pressure) + " hPa \n";
  return message;
}

const char index_html[] PROGMEM = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Video Stream</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      .button {background-color: #2f4468; border: none; color: white; padding: 12px 24px; text-align: center; text-decoration: none; 
        display: inline-block; font-size: 1.8rem; margin: 4px 2px; cursor: pointer; -webkit-touch-callout: none; -webkit-user-select: none; 
        -khtml-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; -webkit-tap-highlight-color: rgba(0,0,0,0);}
      img {  width: auto ; max-width: 400px ; height: auto ; padding-top: 20px;}
      html {font-family: Arial; display: inline-block; text-align: center;}
      body {  margin: 0;}
      .topnav { overflow: hidden; background-color: #0c1b35; color: white; font-size: 1.7rem; }
      .content { padding: 12px; }
      .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
      .cards { max-width: 300px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
      .reading { font-size: 1.8rem; }
      .card.temperature { color: #3fca6b; }
      .card.humidity { color: #17bebb; }
      .card.pressure { color: #4b1d3f; }
    </style>
  </head>
  <body>
    <div class="topnav">
      <h3>ESP32-CAM Video Stream</h3>
    </div>
    <img src="" id="photo" >
    <div align="center">
      <button class="button" onclick="startStream('start');">START</button>
      <button class="button" onclick="startStream('stop');">STOP</button>
    </div>
    <div class="content">
      <div class="cards">
        <div class="card temperature">
          <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
        </div>
        <div class="card humidity">
          <h4><i class="fas fa-tint"></i> HUMIDITY</h4><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
        </div>
        <div class="card pressure">
          <h4><i class="fas fa-angle-double-down"></i> PRESSURE</h4><p><span class="reading"><span id="pres">%PRESSURE%</span> hPa</span></p>
        </div>
      </div>
    </div>
    <script>
    function startStream(x) {
      if(x=='start'){
        document.getElementById("photo").src = window.location.href + "stream";
      }
      else if(x=='stop'){
        document.getElementById("photo").src = "";
      } 
    }
    window.onload = document.getElementById("photo").src = window.location.href + "stream";
    if (!!window.EventSource) {
       var source = new EventSource('/events');  
       source.addEventListener('open', function(e) {
        console.log("Events Connected");
       }, false);
       source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
       }, false);
       source.addEventListener('message', function(e) {
        console.log("message", e.data);
       }, false);
       source.addEventListener('temperature', function(e) {
        console.log("temperature", e.data);
        document.getElementById("temp").innerHTML = e.data;
       }, false); 
       source.addEventListener('humidity', function(e) {
        console.log("humidity", e.data);
        document.getElementById("hum").innerHTML = e.data;
       }, false);
       source.addEventListener('pressure', function(e) {
        console.log("pressure", e.data);
        document.getElementById("pres").innerHTML = e.data;
       }, false);   
    }
  </script>
  </body>
</html>
)rawliteral";

String processor(const String& var){
 getReadings();
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  else if(var == "PRESSURE"){
    return String(pressure);
  }
}

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

AsyncWebServer server(80);
AsyncEventSource events("/events");

void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  // Init BME280 sensor
  Wire.begin(I2C_SDA, I2C_SCL);
  bme.settings.commInterface = I2C_MODE;
  bme.settings.I2CAddress = 0x76;
  bme.settings.runMode = 3;
  bme.settings.tStandby = 0;
  bme.settings.filter = 0;
  bme.settings.tempOverSample = 1;
  bme.settings.pressOverSample = 1;
  bme.settings.humidOverSample = 1;
  bme.begin();
  
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
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 15;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  
  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/stream", HTTP_GET, streamJpg);

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  
  server.addHandler(&events);
  server.begin();
}

void loop(){
  if ((millis() - lastTime) > timerDelay) {
    String readings = getReadings();
    Serial.println(readings);
    lastTime = millis();
    // Send Events to the Web Server with the Sensor Readings
    events.send("ping", NULL, millis());
    events.send(String(temperature).c_str(), "temperature", millis());
    events.send(String(humidity).c_str(), "humidity", millis());
    events.send(String(pressure).c_str(), "pressure", millis());
  }
}
