// In this program we are publishing and subscribing to a MQTT server that requires a login/password
// authentication scheme. We are connecting with a unique client ID, which is required by the server.
// This unique client ID is derived from our device's MAC address, which is unique to the device, and
// thus unique to the universe.
//
// We have hardcoded the topic and the subtopics in the mqtt.publish() function, because those topics and sub
// topics are never going to change. We have subscribed to the supertopic using the directory-like addressing
// system MQTT provides. To do so, we subscribe to 'super/+' which means we are subscribing to 'super' 
// and every sub-topic that might come after the main topic. We denote this with a '+' symbol.
//
// refactored for ArduinoJSON 6 and revised for clarity
// brc 2020
//
// Modified by emeersman (meerskat) 2020 to publish to different feeds and interact with sensors.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include <ESP8266WiFi.h>    
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h> 
#include <ArduinoJson.h>    

#include "config.h" // include the configuration of credentials in the config.h file

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client

// Char array to store MAC address as unique ID to ping server
char mac[6];

// Declare timers
unsigned long currentMillis, timerOne, timerTwo, timerThree;

const int LED_PIN = 0;
const int BUTTON_PIN = 12;
const int SDA_PIN = 4;
const int SCL_PIN = 5;

boolean button = false;
int buttonCount = 0; // we will report the number of times the button was pressed

/////SETUP/////
void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  setup_wifi(); // start the wifi connection
  mqtt.setServer(mqtt_server, 1883); // MQTT TCP port (unencrypted).
  // so this means all of your data and key are sent in cleartext. not secure, but simple.
  mqtt.setCallback(callback); // to receive MQTT messages, we need to register a callback function

  timerOne = timerTwo = timerThree = millis();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, INPUT);

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}

/// LOOP ///
void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the MQTT connection 'active'

  // Check if 10 seconds have elapsed since last message
  if (millis() - timerOne > 10000) { 
    publishHero();
    timerOne = millis(); // reset the timer
  }

  // Call function to check if button is being pressed and publish to feed if that happens
  checkButton();

  // Check if 15 seconds have elapsed since last message
  if (millis() - timerTwo > 15000) { 
    publishISSLocation();
    timerTwo = millis();  // reset a 15-second timer
  }
}

// Publish a random number on the MQTT super/heroes feed
void publishHero() {
  int heroLevel = random(100);
  
  // However, we store this information in JSON format. Not needed now, but is useful for larger datasets
  StaticJsonDocument<256> outputDoc;     // create the JSON "document"
  outputDoc["Hero Level"] = heroLevel; // add a {key:value} pair to the document
  char buffer[256];                      // store the document into a buffer in memory
  serializeJson(outputDoc, buffer);      // serialize the document

  mqtt.publish(feed1, buffer); // publish the buffer to the feed defined in config.h
}

// When button is pressed, publish the button press count to the MQTT super/mario feed.
void checkButton() {
  int button = digitalRead(BUTTON_PIN);
  if (button == LOW) { // we are looking for the button to go low
    buttonCount += 1; // increment the button count upward

    // Save button press and count to output doc, publish to MQTT feed
    StaticJsonDocument<256> outputDoc;
    outputDoc["marioCoins"]["isGettingCoin"] = !button;
    outputDoc["marioCoins"]["coins"] = buttonCount;
    char buffer[256];
    serializeJson(outputDoc, buffer);
    mqtt.publish(feed2, buffer);
    delay(500); // this is not the best method, but it prevents us from flooding the MQTT server
  }
}

// Call the ISS API to get data on the current location of the ISS and publish to the super/star feed.
void publishISSLocation() {
  // Create an http client and connect to ISS endpoint
  HTTPClient tClient;
  tClient.begin("http://api.open-notify.org/iss-now.json");

  // Request the information from the webserver
  int httpCode = tClient.GET();

  // Ensure that the response was good, otherwise print an error
  if(httpCode == HTTP_CODE_OK) {
    StaticJsonDocument<255> doc;
    StaticJsonDocument<256> outputDoc;
    String response = tClient.getString();

    // Deserialize the JSON response and throw an error if necessary
    DeserializationError error = deserializeJson(doc, response);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with error code ");
      Serial.println(error.c_str());
      Serial.println(response);
      return;
    }

    // Save ISS info to output doc and publish to MQTT
    outputDoc["iss_position"]["latitude"] = doc["iss_position"]["latitude"];
    outputDoc["iss_position"]["longitude"] = doc["iss_position"]["longitude"];
    char buffer[256];
    serializeJson(outputDoc, buffer);
    mqtt.publish(feed3, buffer);
  } else {
    Serial.println("Error connecting to ISS endpoint.");
  }

  // Close the http connection
  tClient.end();
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);  // wait 500ms to establish connection
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.

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
      mqtt.subscribe(feed1);
      mqtt.subscribe(feed2);
      mqtt.subscribe(feed3);
    } else {                               //please change 'feeds/' to reflect your user/topics you are subscribing to
      Serial.print("Connection failed, because: ");
      Serial.print(mqtt.state());
      Serial.println(". Will try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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
