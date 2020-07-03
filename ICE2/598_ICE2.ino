// Code largely sourced from Adafruit IO Digital Input Example
// Tutorial Link: https://learn.adafruit.com/adafruit-io-basics-digital-input
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// Include config file which defines Adafruit IO and WiFi 
// credentials.
#include "config.h"

/************************ Example Starts Here *******************************/

// Define digital pin constants
#define BUTTON_PIN 4
#define BLUE_LED_PIN 2
#define GREEN_LED_PIN 5

// Define analog pin constants
#define POT_PIN A0

// Variables to capture button/mag sensor state
bool current = false;
bool last = false;

// Variables to capture potentiometer state
int currentPotVal = 0;
int lastPotVal = 0;

// Set up button/mag sensor and potentiometer feeds in Adafruit
AdafruitIO_Feed *digital = io.feed("Digital Input");
AdafruitIO_Feed *pot = io.feed("Potentiometer");

void setup() {
  // Set button and potentiometer as input and LEDs as output
  pinMode(BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);

  // Start the serial connection
  Serial.begin(9600);

  // Wait for serial monitor to open
  while(! Serial);

  // Connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // Wait for a connection to Adafruit, print a "." every
  // half second while waiting
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // Print that Adafruit connection has been established
  Serial.println();
  Serial.println(io.statusText());
}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // Read and save the current state of the button.
  if(digitalRead(BUTTON_PIN) == HIGH) {
    current = true;
  } else {
    current = false;
  }

  // Read and save the current state of the potentiometer.
  currentPotVal = analogRead(POT_PIN);

  // Turn on both LEDs if button is pressed, otherwise turn one on
  // if potentiometer is in the correct range. If none of these
  // conditions are true, turn off both LEDs.
  if (current) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BLUE_LED_PIN, HIGH);
  } else if (currentPotVal < 300) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BLUE_LED_PIN, LOW);
  } else if (currentPotVal > 700) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
  }

  // Check if any sensor values have changed since being published
  // to Adafruit I/O. If so, update that sensor value in its feed.
  if (current != last && currentPotVal != lastPotVal) {
    // Save the current potentiometer state to the corresponding feed.
    Serial.print("sending pot value -> ");
    Serial.println(currentPotVal);
    pot->save(currentPotVal);
    lastPotVal = currentPotVal;
    
    // Save the current button state to the corresponding feed.
    Serial.print("sending button -> ");
    Serial.println(current);
    digital->save(current);
    // Store last button state.
    last = current;
  } else if (current != last) {
    // Save the current button state to the corresponding feed.
    Serial.print("sending button -> ");
    Serial.println(current);
    digital->save(current);
    // Store last button state.
    last = current;
  } else if (abs(currentPotVal - lastPotVal) > 200) {
    // Save the current potentiometer state to the corresponding feed.
    Serial.print("sending pot value -> ");
    Serial.println(currentPotVal);
    pot->save(currentPotVal);
    lastPotVal = currentPotVal;
  }
}
