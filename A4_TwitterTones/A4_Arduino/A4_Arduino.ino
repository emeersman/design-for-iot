// Create a Twitter sentiment analysis platform that performs sentiment analysis on tweets about 
// two topics and maps those sentiment values to RGB LEDs. The topics are displayed on an OLED screen.
// The topics and sentiment analyses are published to MQTT feeds. Any subscriber could change the
// current query topics and/or receive the sentiment data.
//
// This project invokes the Watson Tone Analyzer API (https://www.ibm.com/watson/services/tone-analyzer/)
// and the Twitter Developer API (https://developer.twitter.com/en/docs/basics/getting-started).
//
// Written by emeersman 2020

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Include the configuration of credentials in the config.h file
#include "config.h"

// Define digital pin constants
#define BUTTON_PIN 3
#define SDA_PIN 4
#define SCL_PIN 5

#define RIGHT_RED_PIN 12
#define RIGHT_GREEN_PIN 13
#define RIGHT_BLUE_PIN 14

#define LEFT_RED_PIN 15
#define LEFT_GREEN_PIN 0
#define LEFT_BLUE_PIN 2

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;             //espClient
PubSubClient mqtt(mqtt_server, 1883, callback, espClient);     //tie PubSub (mqtt) client to WiFi client

// Char array to store MAC address as unique ID to ping server
char mac[6];

// Declare timer
unsigned long twitterTimer;

// Struct representing RGB values, used for easily manipulating RGB LEDs.
struct RGB {
  int red;
  int green;
  int blue;
};

// Struct containing all possible sentiments returned from IBM Tone Analyzer API.
struct Sentiments {
  RGB anger;
  RGB fear;
  RGB joy;
  RGB sadness;
  RGB analytical;
  RGB confident;
  RGB tenative;
  RGB neutral;
};

// Map of sentiments to RGB values, used to set LED colors accordingly.
Sentiments sentimentDict = {
  {255, 0, 0}, // anger, red
  {128, 0, 128}, // fear, purple
  {255, 140, 0}, // joy, yellow
  {0, 0, 255}, // sadness, blue
  {0, 255, 255}, // analytical, aqua
  {255, 50, 0}, // confident, orange
  {0, 255, 0}, // tenative, green
  {255, 255, 255}, // white, neutral
};

// Global variable tracking both Twitter queries, contains default values.
String firstTwitterQuery = "dogs";
String secondTwitterQuery = "cats";

// Tracks whether the current queries have received a sentiment response.
boolean receivedResponse = true;

// Number of seconds between each Twitter query
const int QUERY_RESET_TIME = 30;

/////SETUP/////
void setup() {
  Serial.begin(115200);

  setup_wifi(); // start the wifi connection

  // Initialize timer and I/O pins
  twitterTimer = millis();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_RED_PIN, OUTPUT);
  pinMode(RIGHT_GREEN_PIN, OUTPUT);
  pinMode(RIGHT_BLUE_PIN, OUTPUT);
  pinMode(LEFT_RED_PIN, OUTPUT);
  pinMode(LEFT_GREEN_PIN, OUTPUT);
  pinMode(LEFT_BLUE_PIN, OUTPUT);

  // set analogWrite range for ESP8266
  #ifdef ESP8266
    analogWriteRange(255);
  #endif

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display(); // Show initial display buffer - Adafruit splash screen
  delay(2000); // Pause for 2 seconds
  display.clearDisplay(); // Clear the buffer

  // Initialize LEDs
  setRGB(sentimentDict.neutral, false);
  setRGB(sentimentDict.neutral, true);

  // Initialize values on display
  reconnect();
  updateTwitterQuery();
  drawText();
}

/// LOOP ///
void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the MQTT connection 'active'

  // Call the Twitter update function a set number of times to avoid rate limits, only
  // call it if we are not waiting on a request.
  if (receivedResponse && millis() - twitterTimer > (QUERY_RESET_TIME * 1000)) {
    updateTwitterQuery();
    twitterTimer = millis(); // reset the timer
  }

  // This function is irrelevant to A4 - please ignore
  // Call function to check if button is being pressed and publish to feed if that happens
  turnOnLights();
}

// Publish the current Twitter queries to the relevant feed.
void updateTwitterQuery() {
  DynamicJsonDocument outputDoc(2048);
  char buffer[256];

  // Store k/v pairs into the Json doc, then serialize
  outputDoc["q1"] = firstTwitterQuery;
  outputDoc["q2"] = secondTwitterQuery;
  serializeJson(outputDoc, buffer);

  // Publish the buffer to an MQTT feed
  mqtt.publish(twitterQueryFeed, buffer, true);
  receivedResponse = false;
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  // Connect to a WiFi network
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);  // wait 500ms to establish connection
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  // Get the MAC address for later use, when connecting the MQTT server
  //Serial.println(WiFi.macAddress());
  String temp = WiFi.macAddress(); // macAddress() returns a byte array 6 bytes representing the MAC address
  temp.toCharArray(mac, 6);   // these are stored in the clientID char array
}

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as clientID, always unique!!!
      Serial.println("Reconnected!");
      mqtt.subscribe(twitterQueryFeed);
      mqtt.subscribe(twitterResponseFeed);
      mqtt.subscribe(lightsFeed);
    } else {                               //please change 'feeds/' to reflect your user/topics you are subscribing to
      Serial.print("Connection failed, because: ");
      Serial.print(mqtt.state());
      Serial.println(". Will try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Sets a specific RGB LED to a color.
void setRGB(RGB color, boolean isRight) {
  if (isRight) {
    analogWrite(RIGHT_RED_PIN, color.red);
    analogWrite(RIGHT_GREEN_PIN, color.green);
    analogWrite(RIGHT_BLUE_PIN, color.blue);
  } else {
    // My right LED bulb has a weaker red than the left,
    // this adjustment adds a boost to the red values.
    if (color.red + 20 <= 255) {
      color.red += 20;
    }
    analogWrite(LEFT_RED_PIN, color.red);
    analogWrite(LEFT_GREEN_PIN, color.green);
    analogWrite(LEFT_BLUE_PIN, color.blue);
  }
}

// Function to display the current topics on the OLED screen
void drawText() {
  // Clear old information from display buffer
  display.clearDisplay();

  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Print both current queries on the OLED
  display.println(firstTwitterQuery);
  display.setTextSize(2);
  display.println(secondTwitterQuery);

  // Write everything to the OLED display
  display.display();
}

// Bonus - not part of A4
// When button is pressed, publish to a feed to turn on a local Wemo switch
void turnOnLights() {
  int button = digitalRead(BUTTON_PIN);
  if (button == LOW) { // we are looking for the button to go low
    // Save button press and count to output doc, publish to MQTT feed
    StaticJsonDocument<256> outputDoc;
    outputDoc["buttonPressed"] = !button;
    char buffer[256];
    serializeJson(outputDoc, buffer);
    mqtt.publish(lightsFeed, buffer);
    delay(500); // this is not the best method, but it prevents us from flooding the MQTT server
  }
}
