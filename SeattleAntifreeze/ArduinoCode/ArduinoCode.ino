/*
    Display historical temperature data for the location of an ESP8266 on an LED strip. The
    strip will display historical high temperature values (converted to RGB) for the current
    month/year over the past ~40 years (since 1979). Historical values are obtained via an
    MQTT feed which pulls from a JSON file downloaded from OpenWeather (https://openweathermap.org/api).
    The strip also supports modes which display the 7-day forecast and hourly forecast for the
    current day. This data also comes from OpenWeather. To get the ESP8266 location, the code uses
    APIify (http://api.ipify.org/), to request the IP address and the ipstack API (http://api.ipstack.com/)
    to request Geolocation data. Users can switch through modes by pressing a button and
    data for the current mode is displayed on an OLED screen. LED strip brightness is determined
    by a photocell on the board.

    Written by emeersman (www.github.com/emeersman) 2020
*/

//ArduinoJson can decode Unicode escape sequence, but this feature is disabled by default
//because it makes the code bigger. To enable it, you must define ARDUINOJSON_DECODE_UNICODE to 1
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h> //provides the ability to parse and construct JSON objects

#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

// Include the configuration of credentials in the config.h file
#include "config.h"

#define SDA_PIN 4
#define SCL_PIN 5
#define BUTTON_PIN 12
#define LED_PIN 2
#define PHOTOCELL_PIN A0

// Number of Neopixels on the LED strip
#define LED_COUNT 60

// Declare the NeoPixel strip object
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

// Define struct for holding geolocation data from ipstack API
typedef struct {
  String ip;
  String c_code;
  String c_name;
  String city;
  String lat;
  String lng;
} GeoData;

// Create location instance of GeoData to hold geolocation of ESP8266
GeoData location;

// Enum containing all possible display states for the LED strip.
enum DisplayMode {
  DAILY,
  HISTORICAL,
  OFF,
};

// Initialize LED strip to off
DisplayMode currentMode = OFF;

// Establish WiFi client and connect it with MQTT client
WiFiClient espClient;
PubSubClient mqtt(mqtt_server, 1883, callback, espClient);

// Brightness value read from the photocell
int brightness = 0;

// Threshold over which to update brightness value
const int BRIGHTNESS_THRESHOLD = 20;

// Timezone offset for Seattle
const int timeZoneOffsetInHours = 8;
const long utcOffsetInSeconds = -28800;

// Time constants in milliseconds
const long msInADay = 86400000;
const long msInAnHour = 3600000;
const long msInAMinute = 60000;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Char array to store MAC address as unique ID to ping server
char mac[6];

// Declare timers
unsigned long updateTimer, debounceTimer, dailyTimer, photocellTimer;
// Debounce time between button presses
unsigned long debounceDelay = 50;

// Detects whether the code has published the first daily temperature forecast since initialization
boolean hasPublishedFirstTemp = false;

// Max temperatures for the current and previous days
float dailyMaxTemp = 0.0;
float prevDailyMaxTemp = 0.0;
float currentTemp = 0.0;

// Arrays storing the current temperature data for each mode
float dailyArray[8];
// TODO (future): Modify length to be equal to current year minus starting 
// year of historical data (1979)
float historicalArray[42];

// Setup function
void setup() {
  Serial.begin(115200);
  // Start the WiFi connection
  setup_wifi();

  // Set analogWrite range for ESP8266
  #ifdef ESP8266
    analogWriteRange(255);
  #endif

  // Initialize timers and ESP8266 pins
  updateTimer = debounceTimer = photocellTimer = millis();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize the NeoPixel strip object
  strip.begin();
  // Turn off all pixels on the strip
  strip.show();
  // Set brightness to 1/5 (max 255)
  strip.setBrightness(50);

  // Initialize the time client
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // Unix time adjusted for time zone
  long hours = long((23 - timeClient.getHours()) * msInAnHour);
  long minutes = long((59 - timeClient.getMinutes()) * msInAMinute);
  long seconds = long((62 - timeClient.getSeconds()) * 1000);

  // Calculation to figure out the time until midnight so the daily
  // timer can publish the temperature to an MQTT feed starting at
  // midnight
  long msUntilMidnight = hours + minutes + seconds;
  dailyTimer = msInADay - msUntilMidnight;

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display(); // Show initial display buffer - Adafruit splash screen
  delay(500); // Pause for half a second
  display.clearDisplay(); // Clear the buffer
  display.display();

  // Get geolocation for the ESP8266 once on startup
  reconnect();
  getGeolocation();

  // Get current temperature and forecast data
  getOpenWeatherData();
  // Send a query to populate the historical data for the current date
  mqtt.publish(queryFeed, "get historical data");
  updateDisplay();
}

// Loop function.
void loop() {
  timeClient.update();
  if (!mqtt.connected()) {
    reconnect();
  }

  // Read from the photocell twice a second - constantly reading from the analog pin disrupts the wifi and 
  // frequently drops the MQTT connection
  if (millis() - photocellTimer > 500) {
    // Update brightness on LED strip when current photocell value is far enough away from the existing brightness
    int currBrightness = analogRead(PHOTOCELL_PIN);
    if (abs(currBrightness - brightness) > BRIGHTNESS_THRESHOLD) {
      brightness = currBrightness;
      // Subtract from max reading value because LED strip should get brighter if the sensor input is darker
      currBrightness = 1024 - currBrightness;
      // Map brightness reading to LED brightness scale
      int scaledBrightness = map(currBrightness, 0, 1024, 0, 255);
      strip.setBrightness(scaledBrightness);
    }
    photocellTimer = millis();
  }

  // Every day at midnight, publish the temperature forecast for the day to the MQTT feed
  if (hasPublishedFirstTemp && millis() - dailyTimer > msInADay) {
    publishTemp();
  } else if (!hasPublishedFirstTemp && millis() + dailyTimer > msInADay) {
    hasPublishedFirstTemp = true;
    publishTemp();
  }

  // Detect button presses and cycle through modes every time the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Execute for button press if last press was more than debounce time ago
    if ((millis() - debounceTimer) > debounceDelay) {
      // Cycle through the modes each time the button is pressed
      switch (currentMode) {
        case OFF:
          currentMode = DAILY;
          break;
        case DAILY:
          currentMode = HISTORICAL;
          break;
        default:
          currentMode = OFF;
          break;
      }
      updateDisplay();
    }
    // Reset debounce timer whenever button gets pressed
    debounceTimer = millis();
  }

  // Update the data if we're in a display mode that needs to be updated and we haven't updated the data in more than an hour
  if (millis() - updateTimer > msInAnHour && currentMode == DAILY) {
    // Update the weather data for daily forecasts
    getOpenWeatherData();
    updateDisplay();
    updateTimer = millis(); // reset the timer
  }

  mqtt.loop(); //this keeps the MQTT connection 'active'
}

// Update the OLED display and LED strip depending on the current mode
void updateDisplay() {
  strip.clear();
  switch (currentMode) {
    case HISTORICAL:
      displayHistoricalData();
      break;
    case DAILY:
      displayDailyData();
      break;
    case OFF:
      display.clearDisplay();
      display.display();
      strip.clear();
      break;
    default:
      break;
  }
}

// Publish the current and previous days' temperatures to an MQTT feed
void publishTemp() {
  // Reset timer
  dailyTimer = millis();
  // Get updated forecast values
  getOpenWeatherData();
  getPrevDayData();

  // Load temperature values into a JSON document
  DynamicJsonDocument outputDoc(2048);
  outputDoc["temperature"] = dailyMaxTemp;
  outputDoc["prev_temperature"] = prevDailyMaxTemp;

  char buffer[256];
  serializeJson(outputDoc, buffer);
  mqtt.publish(dailyFeed, buffer);
  prevDailyMaxTemp = 0.0;
}

// Get the max temperature for the previous day, accounting for timezone offsets.
void getPrevDayData() {
  // Create http clients and connect to OpenWeather endpoint
  HTTPClient owClient;
  // Call the OpenWeather API twice to get previous day weather due to GMT offset
  long currDayUtc = timeClient.getEpochTime();
  long prevDayUtc = currDayUtc - 86400;

  owClient.begin("https://api.openweathermap.org/data/2.5/onecall/timemachine?lat=" + location.lat + "&lon=" + location.lng + "&dt=" + prevDayUtc + "&units=imperial&appid=" + weather_api_key, "EE AA 58 6D 4F 1F 42 F4 18 5B 7F B0 F2 0A 4C DD 97 47 7D 99");

  int httpCode = owClient.GET();

  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    // Deserialize the JSON response using a filter
    DynamicJsonDocument filter(2048);
    filter["hourly"][0]["temp"] = true;
    deserializeJson(doc, owClient.getString(), DeserializationOption::Filter(filter));

    // Iterate through the max temperatures from the previous day (GMT) and save the highest
    float currMaxTemp = 0.0;
    JsonArray prevDayObjects = doc["hourly"].as<JsonArray>();
    int count = 0;
    for (JsonObject prevDayObject : prevDayObjects) {
      if (count > timeZoneOffsetInHours) {
        float hourTemp = prevDayObject["temp"].as<float>();
        if (hourTemp > currMaxTemp) {
          currMaxTemp = hourTemp;
        }
      }
      count += 1;
    }
    prevDailyMaxTemp = currMaxTemp;
  } else {
    Serial.println("Error connecting to OpenWeather endpoint.");
  }
  // Close the http connections
  owClient.end();

  owClient.begin("https://api.openweathermap.org/data/2.5/onecall/timemachine?lat=" + location.lat + "&lon=" + location.lng + "&dt=" + currDayUtc + "&units=imperial&appid=" + weather_api_key, "EE AA 58 6D 4F 1F 42 F4 18 5B 7F B0 F2 0A 4C DD 97 47 7D 99");
  // Request the information from the webserver
  int currHttpCode = owClient.GET();

  // Ensure that the response was good, otherwise print an error
  if (currHttpCode == HTTP_CODE_OK) {
    DynamicJsonDocument currDoc(2048);
    // Deserialize the JSON response using a filter
    DynamicJsonDocument filter(2048);
    filter["hourly"][0]["temp"] = true;
    deserializeJson(currDoc, owClient.getString(), DeserializationOption::Filter(filter));

    // Iterate through the max temperatures from the current day (GMT)/previous day (with time
    // zone offset) and save the highest
    float currMaxTemp = 0.0;
    JsonArray currDayObjects = currDoc["hourly"].as<JsonArray>();
    int count = 0;
    for (JsonObject currDayObject : currDayObjects) {
      float currTemp = currDayObject["temp"].as<float>();
      if (currTemp > currMaxTemp) {
        currMaxTemp = currTemp;
      }
      count += 1;
    }
    prevDailyMaxTemp = max(prevDailyMaxTemp, currMaxTemp);
  } else {
    Serial.println("Error connecting to OpenWeather endpoint.");
  }
  // Close the http connections
  owClient.end();
}

// Call the OpenWeather API to get the weather forecast for the current location.
void getOpenWeatherData() {
  // Create an http client and connect to OpenWeather endpoint
  HTTPClient owClient;
  owClient.begin("https://api.openweathermap.org/data/2.5/onecall?lat=" + location.lat + "&lon=" + location.lng + "&exclude=minutely,hourly&units=imperial&appid=" + weather_api_key, "EE AA 58 6D 4F 1F 42 F4 18 5B 7F B0 F2 0A 4C DD 97 47 7D 99");
  // Request the information from the webserver
  int httpCode = owClient.GET();

  // Ensure that the response was good, otherwise print an error
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    // Deserialize the JSON response using a filter
    DynamicJsonDocument filter(2048);
    filter["daily"][0]["temp"]["max"] = true;
    filter["current"]["temp"] = true;
    deserializeJson(doc, owClient.getString(), DeserializationOption::Filter(filter));

    // Parse hourly and daily forecasts out of API response into global arrays
    JsonArray dailyObjects = doc["daily"].as<JsonArray>();
    int dailyCount = 0;
    for (JsonObject dailyObject : dailyObjects) {
      dailyArray[dailyCount] = dailyObject["temp"]["max"].as<float>();
      dailyCount += 1;
    }
    dailyMaxTemp = doc["daily"][0]["temp"]["max"];
    currentTemp = doc["current"]["temp"].as<float>();
  } else {
    Serial.println("Error connecting to OpenWeather endpoint.");
  }
  // Close the http connection
  owClient.end();
}

// Display the daily data on the LED strip and OLED screen
void displayDailyData() {
  int sizeOfTemps = 56;
  float dailyTemps[sizeOfTemps];
  float maxVal = 0.0;
  float minVal = 100.0;
  int maxDay = 0;
  int minDay = 0;
  int count = 0;
  // Loop through daily data objects
  for (float temperature : dailyArray) {
    // Allocate multiple LEDs per day, make a long array
    for (int i = 0; i < 7; i++) {
      if(temperature > maxVal) {
        maxVal = temperature;
        maxDay = count;
      } else if (temperature < minVal) {
        minVal = temperature;
        minDay = count;
      }
      dailyTemps[(count * 7) + i] = temperature;
    }
    count++;
  }
  drawText("daily", currentTemp, maxVal, minVal, maxDay, minDay);
  tempsToLEDStrip(dailyTemps, sizeof(dailyTemps), sizeOfTemps);
}

// Display the historical data for the current date on the LED strip and OLED screen
void displayHistoricalData() {
  int sizeOfTemps = 42;
  float historicalTemps[sizeOfTemps];
  float maxVal = 0.0;
  float minVal = 100.0;
  int maxYear = 0;
  int minYear = 0;
  int earliestYear = 1979;
  int count = 0;
  // Loop through historical yearly data objects
  for (float temperature : historicalArray) {
    historicalTemps[count] = temperature;
    if(temperature > maxVal) {
      maxVal = temperature;
      maxYear = count + earliestYear;
    } else if (temperature < minVal) {
      minVal = temperature;
      minYear = count + earliestYear;
    }
    count++;
  }
  drawText("hist.", dailyMaxTemp, maxVal, minVal, maxYear, minYear);
  tempsToLEDStrip(historicalTemps, sizeof(historicalTemps), sizeOfTemps);
}

// Convert an array of floats to RGB values and write them to the LED strip
void tempsToLEDStrip(float temps[], int sizeOfTemps, int intSizeOfTemps) {
  int startingIndex = (strip.numPixels() - intSizeOfTemps) / 2;
  int currPixel = startingIndex;
  for (int i = 0; i < intSizeOfTemps; i++) {
    float temp = temps[i];
    // turn float into rgb color
    uint32_t color = floatToRgb(temp, 20, 100);
    strip.setPixelColor(currPixel, color);
    strip.show();
    currPixel += 1;
  }
}

// Convert a numerical value along a given scale to a string hex value
// ranging from blue (low/cold) to red (high/hot).
// Sourced from https://learning.oreilly.com/library/view/python-cookbook/0596001673/ch09s11.html
// Modified RGB equations in floatRGB
uint32_t floatToRgb(float mag, int cmin, int cmax) {
  // Normalize to 0-1
  float x = float(mag - cmin) / float(cmax - cmin);
  int green = 0;
  if (x <= 0.5) {
    green = int(255.0 * min(max(2.0 * x, 0.0), 1.0));
  } else {
    green = int(255.0 * min(max(2.0 * (0.9 - x), 0.0), 1.0));
  }
  int red = int(255.0 * min(max(4.0 * (x - 0.25), 0.0), 1.0));
  int blue = int(255.0 * min(max(4.0 * fabsf(x - 0.75) - 1.0, 0.0), 1.0));
  return strip.Color(red, green, blue);
}

// Function to display the current mode data on the OLED screen
void drawText(String dataType, float current, float high, float low, int highIndex, int lowIndex) {
  // Clear old information from display buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Display mode and current location
  display.print(location.city);
  display.print(" ");
  display.print(dataType);
  display.println(" temps");

  // Display current temp for the day or for the hour
  if (dataType.equals("daily")) {
    display.print("Current: ");
  } else {
    display.print("Today: ");
  }
  display.print(current);
  display.println("*F");

  // Display the historical or forecast highs and lows as well as the day/year when they occur(red)
  display.print("High: ");
  display.print(high);
  display.print("*F (");
  display.print(highIndex);
  display.println(")");

  display.print("Low: ");
  display.print(low);
  display.print("*F (");
  display.print(lowIndex);
  display.println(")");
  
  // Write everything to the OLED display
  display.display();
}

// Set up the WiFi connection for the ESP8266 device.
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
  String temp = WiFi.macAddress(); // macAddress() returns a byte array 6 bytes representing the MAC address
  temp.toCharArray(mac, 6);   // these are stored in the clientID char array
}

// Monitor the connection to the MQTT server and reconnect if it goes down.
void reconnect() {
  // Loop until it reconnects
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) {
      Serial.println("Reconnected!");
      mqtt.subscribe(locationFeed);
      mqtt.subscribe(historicalFeed);
      mqtt.subscribe(dailyFeed);
    } else {
      Serial.print("Connection failed, because: ");
      Serial.print(mqtt.state());
      Serial.println(". Will try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Return IP address for ESP8266 board.
String getIP() {
  HTTPClient ipClient;
  String ipAddress;

  ipClient.begin("http://api.ipify.org/?format=json");
  int httpCode = ipClient.GET();

  if (httpCode == 200) {
    StaticJsonDocument<100> doc;
    String payload = ipClient.getString();
    deserializeJson(doc, payload);
    ipAddress = doc["ip"].as<String>();
  } else {
    Serial.println("Something went wrong with connecting to the endpoint.");
    ipAddress = "error";
  }
  ipClient.end();
  return ipAddress;
}

// Get the geolocation from ipstack API and publish the city name to an MQTT feed.
void getGeolocation() {
  HTTPClient geoClient;
  geoClient.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + geolocation_key); //return IP as .json object

  int httpCode = geoClient.GET();

  if (httpCode == 200) {
    DynamicJsonDocument doc(2048);
    String payload = geoClient.getString();

    // Deserialize the JSON response and throw an error if necessary
    DeserializationError error = deserializeJson(doc, payload);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with error code ");
      Serial.println(error.c_str());
      Serial.println(payload);
      return;
    }

    // Save data from response to GeoData object
    location.ip = doc["ip"].as<String>();
    location.c_code = doc["country_code"].as<String>();
    location.c_name = doc["country_name"].as<String>();
    location.city = doc["city"].as<String>();
    location.lat = doc["latitude"].as<String>();
    location.lng = doc["longitude"].as<String>();

    Serial.println("External IP address is " + location.ip);
    Serial.print("Currently in " + location.c_name);
    Serial.println(" in or near " + location.city);
    Serial.println("at approximately " + location.lat + " latitude by " + location.lng + " longitude.");

    DynamicJsonDocument outputDoc(2048);
    outputDoc["city_name"] = location.city;
    char buffer[256];
    serializeJson(outputDoc, buffer);

    // Publish location data to MQTT feed
    mqtt.publish(locationFeed, buffer, true);
  } else {
    Serial.println("Something went wrong with connecting to the endpoint.");
  }
  geoClient.end();
}
