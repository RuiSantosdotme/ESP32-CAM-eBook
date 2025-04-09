/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

// flashlight is connected to GPIO 4
const int ledPin = 4;

// setting PWM properties
const int freq = 5000;
const int resolution = 8;
const int channel = 0;

void setup(){
  // configure LED PWM functionalities
  ledcAttachChannel(ledPin, freq, resolution, channel);
}

void loop(){
  // increase the LED brightness
  for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){
    // changing the LED brightness with PWM
    ledcWrite(ledPin, dutyCycle);
    delay(15);
  }
  // decrease the LED brightness
  for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
    // changing the LED brightness with PWM
    ledcWrite(ledPin, dutyCycle);
    delay(15);
  }
}
