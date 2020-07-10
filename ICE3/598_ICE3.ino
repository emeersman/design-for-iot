// Adafruit IO Temperature & Humidity Example
// Tutorial Link: https://learn.adafruit.com/adafruit-io-basics-temperature-and-humidity
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit an0d open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016-2017 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

// Code based heavily off of the above example as well as the
// SSD1306 example:
// https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x32_i2c/ssd1306_128x32_i2c.ino
// as well as example DHT22 code originally provided by Adafruit
// and modified by Brock Craft:
// https://gist.github.com/brockcraft/d37928610e9562a4c47c8949f348d9a0

// The Adafruit I/O dashboard referenced in this code can be viewed 
// at: https://io.adafruit.com/meerskat/dashboards/ice3

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Code Starts Here *******************************/

#include <Adafruit_GFX.h>
#include <Adafruit_MPL115A2.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SPI.h>
#include <Wire.h>

// Define digital pin constants
#define BUTTON_PIN 2
#define SDA_PIN 4
#define SCL_PIN 5
#define TEMP_PIN 12

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create temperature/pressure sensor instance
Adafruit_MPL115A2 mpl115a2;

// Create temperature/humidity sensor instance
DHT_Unified dht(TEMP_PIN, DHT22);

// Set up Adafruit I/O feeds to display sensor data
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *mplTemperature = io.feed("mplTemperature");
AdafruitIO_Feed *humidity = io.feed("humidity");
AdafruitIO_Feed *pressure = io.feed("pressure");
AdafruitIO_Feed *btnFeed = io.feed("button");

// Variable to capture which temperature format to display
bool isFahrenheit = true;

void setup() {
  // Start the serial connection
  Serial.begin(9600);

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);

  // Wait for serial monitor to open
  while(! Serial);

  // Initialize dht22
  dht.begin();

  // Initialize mpl115a2
  Serial.println("Getting barometric pressure ...");
  mpl115a2.begin();

  // Connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // Set up a message handler for the button feed, will call
  // buttonPressed function whenever the dashboard button is pressed
  btnFeed->onMessage(buttonPressed);
  
  // Wait for a connection to Adafruit, print a "." every
  // half second while waiting
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // Print that Adafruit connection has been established
  Serial.println();
  Serial.println(io.statusText());

  // I don't know what this does, but it was in the Adafruit example
  btnFeed->get();
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // Read temperature from DHT22 sensor
  sensors_event_t event;
  dht.temperature().getEvent(&event);  

  // Convert raw temperature data to celsius and fahrenheit
  float celsius = event.temperature;
  float fahrenheit = (celsius * 1.8) + 32;

  // Publish temperature data to Adafruit IO
  temperature->save(fahrenheit);

  // Read temperature from MPL115A2 sensor
  float temperatureC = 0; 
  temperatureC = mpl115a2.getTemperature(); 

  // Convert temperature to fahrenheit
  float temperatureF = (temperatureC * 1.8) + 32;

  // Publish temperature data to Adafruit IO
  mplTemperature->save(temperatureF);

  // Read humidity from DHT22 sensor and publish to Adafruit IO
  dht.humidity().getEvent(&event);
  float humidityPct = event.relative_humidity;
  humidity->save(humidityPct);

  // Read pressure data from MPL115A2 sensor and publish to Adafruit IO
  float pressureKPA = 0;
  pressureKPA = mpl115a2.getPressure();
  pressure->save(pressureKPA);
  
  // Toggle the temperature units whenever the button is pressed
  if (digitalRead(BUTTON_PIN) == HIGH) {
    isFahrenheit = !isFahrenheit;
    // Wait 1 second between detecting button presses
    delay(1000);
  }

  // Call drawText function to write current data to OLED screen
  if (isFahrenheit) {
    drawText(fahrenheit, temperatureF, humidityPct, pressureKPA);
  } else {
    drawText(celsius, temperatureC, humidityPct, pressureKPA);
  }

  // Wait 8 seconds to avoid Adafruit IO rate limit
  // To better detect button presses, this should be replaced
  // with a check to only write/publish each sensor value if it 
  // changes by a set amount.
  delay(8000);
}

// Function to display the current sensor data on the OLED screen
void drawText(float temperature, float mplTemp, float humidity, float pressure) {
  // Clear old information from display buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Display DHT22 temp on OLED
  display.print("Temp: ");
  display.print(temperature);
  if (isFahrenheit) {
    display.println("*F");
  } else {
    display.println("*C");
  }
  
  // Display MPL115A2 temp on OLED
  display.print("MPL temp: ");
  display.print(mplTemp);
  if (isFahrenheit) {
    display.println("*F");
  } else {
    display.println("*C");
  }

  // Display humidity on OLED
  display.print("Humidity: ");
  display.print(humidity);
  display.println("%");

  // Display pressure on OLED
  display.print("Pressure: ");
  display.print(pressure);
  display.println("kPa");

  // Write everything to the OLED display
  display.display();
}

// This function is called whenever a btnFeed message is received
// from Adafruit IO. It does not do anything with the actual data
// object but rather toggles between fahrenheit and celsius in this
// code.
void buttonPressed(AdafruitIO_Data *data) {
  isFahrenheit = !isFahrenheit;
}
