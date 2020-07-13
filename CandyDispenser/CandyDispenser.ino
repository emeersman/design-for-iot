// Some of this code is based off of the Adafruit servo example here:
// https://learn.adafruit.com/adafruit-arduino-lesson-14-servo-motors/arduino-code-for-sweep
// as well as the Adafruit OLED example here:
// https://github.com/adafruit/Adafruit_SSD1306/tree/master/examples/ssd1306_128x32_i2c
// 
// The Adafruit I/O dashboard referenced in this code can be viewed 
// at: https://io.adafruit.com/meerskat/dashboards/candy-dispenser

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Code Starts Here *******************************/

#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <SPI.h>
#include <Wire.h>

// Define digital pin constants
#define BUTTON_PIN 2
#define SDA_PIN 4
#define SCL_PIN 5
#define SERVO_PIN 16

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create servo instance
Servo servo;

// Variable to capture current servo position in degrees.
int servo_angle = 0;

// Set up Adafruit I/O feeds
AdafruitIO_Feed *dispense = io.feed("dispense");
AdafruitIO_Feed *history = io.feed("candy-history");

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

  // Attach servo to pin and reset angle to 0 degrees.
  servo.attach(SERVO_PIN);
  servo.write(servo_angle);

  // Connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // Set up a message handler for the button feed, will call
  // buttonPressed function whenever the dashboard button is pressed
  dispense->onMessage(dispenseCandy);
  
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
  
  // Dispense candy when the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    dispenseCandy();
    // Wait 1 second between detecting button presses
    delay(1000);
  }

  // Call draw text function with spaces for newline separation on OLED.
  drawText("Push button to       dispense candy");
}

// Function to read button presses from Adafruit IO and call
// candy dispensing function accordingly.
void dispenseCandy(AdafruitIO_Data *data) {
  bool isDispensePressed = data->toPinLevel();
  if (isDispensePressed) {
    dispenseCandy();
  }
}

// Function to handle all actions to dispense candy and publish
// related status to Adafruit IO.
void dispenseCandy() {
  drawText("Dispensing...");
  
  // Swing servo to dispense candy
  servo.write(40);
  delay(1000);
  servo.write(0);
  
  // Publish a dispense event to Adafruit IO
  history->save(0);
  history->save(1);
  history->save(0);
  
  drawText("Enjoy!");
  delay(2000);
}

// Function to display the current sensor data on the OLED screen
void drawText(String textToDisplay) {
  // Clear old information from display buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Display text on OLED
  display.print(textToDisplay);
  
  // Write everything to the OLED display
  display.display();
}
