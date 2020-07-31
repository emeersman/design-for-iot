// Create a weather station that reads temperature, pressure, and humidity data from local sensors
// and publishes the data to an MQTT broker. The broker is running on a Raspberry Pi using
// mosquitto. This program subscribes to all weather feeds and publishes them to an OLED screen 
// wired next to the sensors.
//
// This program uses the API from https://openweathermap.org to get outdoor temperature data for a 
// specified zip code.
//
// Written by emeersman 2020

#include <Adafruit_GFX.h>
#include <Adafruit_MPL115A2.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SPI.h>
#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Include the configuration of credentials in the config.h file
#include "config.h" 

// User-specific info about OpenWeather API key and zip code
const String weather_api_key = "";
const String weather_zip_code = "";

// Define digital pin constants
#define BUTTON_PIN 12
#define SDA_PIN 4
#define SCL_PIN 5
#define TEMP_PIN 2

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create temperature/pressure sensor instance
Adafruit_MPL115A2 mpl115a2;

// Create temperature/humidity sensor instance
DHT_Unified dht(TEMP_PIN, DHT22);

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;             //espClient
PubSubClient mqtt(mqtt_server, 1883, callback, espClient);     //tie PubSub (mqtt) client to WiFi client

// Char array to store MAC address as unique ID to ping server
char mac[6];

// Declare timers
unsigned long indoorTimer, outdoorTimer;

// Weather object to hold all current data displayed on the OLED
struct Weather {
   float fahrenheit;
   float celsius;
   float outdoorFahrenheit;
   float outdoorCelsius;
   float pressure;
   float humidity;
   char location[30];
}; 

// Initialize new Weather object
Weather weather = {0, 0, 0, 0, 0, 0, ""};

// Sensor threshold constants that determine when to update sensor values
const float TEMPERATURE_THRESHOLD = 0.5;
const float PRESSURE_THRESHOLD = 0.3;
const float HUMIDITY_THRESHOLD = 0.5;

/////SETUP/////
void setup() {
  Serial.begin(115200);

  setup_wifi(); // start the wifi connection

  // Initialize timers and button
  indoorTimer = outdoorTimer = millis();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display(); // Show initial display buffer - Adafruit splash screen
  delay(2000); // Pause for 2 seconds
  display.clearDisplay(); // Clear the buffer

  // Initialize dht22 and mpl115a2
  dht.begin();
  mpl115a2.begin();

  // Initialize sensor values on display
  reconnect();
  updateIndoorTemp();
  updateHumidity();
  updatePressure();
  updateOutdoorTemp();
}

/// LOOP ///
void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the MQTT connection 'active'
  
  // Read from sensors twice a minute
  if (millis() - indoorTimer > 30000) {
    updateIndoorTemp();
    updateHumidity();
    updatePressure();
    indoorTimer = millis(); // reset the timer
  }

  // Update outdoor temp every ~15 minutes (avoids excessive calls to external API)
  if (millis() - outdoorTimer > 1000000) {
    updateOutdoorTemp();
    outdoorTimer = millis();
  }

  // This function is irrelevant to A3 - please ignore
  // Call function to check if button is being pressed and publish to feed if that happens
  turnOnLights();
}

// Read the local temperature and publish it to the feed if it is different from the current value in the feed.
void updateIndoorTemp() {
  // Read temperature from DHT22 sensor
  sensors_event_t event;
  dht.temperature().getEvent(&event);  

  // Convert raw temperature data to celsius and fahrenheit
  float currCelsius = event.temperature;
  float currFahrenheit = (currCelsius * 1.8) + 32;

  // Check if humidity data is different from displayed humidity. If so, publish the new value to the feed.
  if (abs(currCelsius - weather.celsius) > TEMPERATURE_THRESHOLD) {    
    StaticJsonDocument<256> outputDoc;     // create the JSON "document"
    outputDoc["indoor_temp"]["fahrenheit"] = currFahrenheit; // add a {key:value} pair to the document
    outputDoc["indoor_temp"]["celsius"] = currCelsius; // add a {key:value} pair to the document
    char buffer[256];                      // store the document into a buffer in memory
    serializeJson(outputDoc, buffer);      // serialize the document
    
    mqtt.publish(indoorTempFeed, buffer, true); // publish the buffer to the feed defined in config.h
  }
}

// Read the local humidity and publish it to the feed if it is different from the current value in the feed.
void updateHumidity() {
  // Read humidity from DHT22 sensor
  sensors_event_t humEvent;
  dht.humidity().getEvent(&humEvent);
  float currHumidity = humEvent.relative_humidity;

  // Check if humidity data is different from displayed humidity. If so, publish the new value to the feed.
  if (abs(currHumidity - weather.humidity) > HUMIDITY_THRESHOLD) {
    StaticJsonDocument<256> outputDoc;     // create the JSON "document"
    outputDoc["humidity_pct"] = currHumidity; // add a {key:value} pair to the document
    char buffer[256];                      // store the document into a buffer in memory
    serializeJson(outputDoc, buffer);      // serialize the document
  
    mqtt.publish(humidityFeed, buffer, true); // publish the buffer to the feed defined in config.h
  }
}

// Read the local pressure and publish it to the feed if it is different from the current value in the feed.
void updatePressure() {
  // Read pressure data from MPL115A2 sensor
  float currPressure = 0;
  currPressure = mpl115a2.getPressure();

  // Check if pressure data is different from displayed pressure. If so, publish the new value to the feed.
  if (abs(currPressure - weather.pressure) > PRESSURE_THRESHOLD) {
    StaticJsonDocument<256> outputDoc;     // create the JSON "document"
    outputDoc["pressure_kpa"] = currPressure; // add a {key:value} pair to the document
    char buffer[256];                      // store the document into a buffer in memory
    serializeJson(outputDoc, buffer);      // serialize the document
  
    mqtt.publish(pressureFeed, buffer, true); // publish the buffer to the feed defined in config.h
  }
}

// Call the OpenWeather API to get the outdoor temperature for a specified zip code.
void updateOutdoorTemp() {
  // Create an http client and connect to OpenWeather endpoint
  HTTPClient tClient;
  tClient.begin("https://api.openweathermap.org/data/2.5/weather?zip=" + weather_zip_code + ",us&appid=" + weather_api_key, "EE AA 58 6D 4F 1F 42 F4 18 5B 7F B0 F2 0A 4C DD 97 47 7D 99");

  // Request the information from the webserver
  int httpCode = tClient.GET();

  // Ensure that the response was good, otherwise print an error
  if(httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    DynamicJsonDocument outputDoc(2048);
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

    // Extract current temperature data and location name from response
    float currCelsius = doc["main"]["temp"].as<float>() - 273.15;
    float currFahrenheit = (currCelsius * 1.8) + 32;
    String currLocation = doc["name"].as<String>();

    // Check if outdoor temperature data is different from displayed temperature. If so, publish the new value to the feed.
    if (abs(currCelsius - weather.outdoorCelsius) > TEMPERATURE_THRESHOLD) {
      // Save response info to output doc and publish to MQTT
      outputDoc["location_name"] = currLocation;
      outputDoc["outdoor_temp"]["fahrenheit"] = currFahrenheit;
      outputDoc["outdoor_temp"]["celsius"] = currCelsius;
      char buffer[256];
      serializeJson(outputDoc, buffer);
      
      mqtt.publish(outdoorTempFeed, buffer, true); // publish the buffer to the feed defined in config.h 
    }
  } else {
    Serial.println("Error connecting to OpenWeather endpoint.");
  }

  // Close the http connection
  tClient.end();
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
      mqtt.subscribe(indoorTempFeed);
      mqtt.subscribe(outdoorTempFeed);
      mqtt.subscribe(humidityFeed);
      mqtt.subscribe(pressureFeed);
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

// Function to display the current sensor data on the OLED screen
void drawText() {
  // Clear old information from display buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Display DHT22 temp on OLED
  display.print("Indoor temp: ");
  display.print(weather.fahrenheit);
  display.println("*F");

  // Display outdoor temp on OLED
  display.print(weather.location);
  display.print(" temp: ");
  display.print(weather.outdoorFahrenheit);
  display.println("*F");
  
  // Display humidity on OLED
  display.print("Humidity: ");
  display.print(weather.humidity);
  display.println("%");

  // Display pressure on OLED
  display.print("Pressure: ");
  display.print(weather.pressure);
  display.println("kPa");

  // Write everything to the OLED display
  display.display();
}

// Bonus - not part of the A3
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
