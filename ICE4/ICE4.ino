/*A sketch to get the ESP8266 on the network and connect to some open services via HTTP to
   get our external IP address and (approximate) geolocative information in the getGeo()
   function. To do this, we will connect to http://api.ipify.org/, to request the IP address
   in JSON format. We then use that IP address when we request the Geolocation data from
   http://api.ipstack.com/  You must request a free API KEY from http://api.ipstack.com/ 
   for this to work. 
   
   This sketch also introduces the flexible type definition struct, which allows us to define
   more complex data structures to make receiving larger data sets a bit cleaner/clearer.

   jeg 2017

   updated to new API format for Geolocation data from ipistack.com
   brc 2019
   refactored to migrate to from ArduinoJSON 5 to ArduinoJSON 6
   brc 2020 

   ////////
   Original code modified by emeersman to support Adafruit I/O interaction
   and ADPS 9960 sensor.
   
   The Adafruit I/O dashboard referenced in this code can be viewed 
   at: https://io.adafruit.com/meerskat/dashboards/geolocation
*/

//ArduinoJson can decode Unicode escape sequence, but this feature is disabled by default
//because it makes the code bigger. To enable it, you must define ARDUINOJSON_DECODE_UNICODE to 1
#define ARDUINOJSON_DECODE_UNICODE 1 
#include <ArduinoJson.h> //provides the ability to parse and construct JSON objects

#include "config.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_APDS9960.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define SDA_PIN 4
#define SCL_PIN 5
#define RED_PIN 15
#define GREEN_PIN 16
#define BLUE_PIN 2

const char* ssid = "***";
const char* pass = "***";
const char* key = "***";

typedef struct { //here we create a new data type definition, a box to hold other data types
  String ip;    //
  String cc;    //for each name:value pair coming in from the service, we will create a slot
  String cn;    //in our structure to hold our data
  String rc;
  String rn;
  String cy;
  String ln;
  String lt;
} GeoData;     //then we give our new data structure a name so we can use it in our code

GeoData location; //we have created a GeoData type, but not an instance of that type,
//so we create the variable 'location' of type GeoData

// Create APDS9960 instance
Adafruit_APDS9960 apds;

// Declare Adafruit IO feeds
AdafruitIO_Feed *geolocation = io.feed("geolocation");
AdafruitIO_Feed *rgbLED = io.feed("rgb-led");

void setup() {
  Serial.begin(115200);
  delay(10);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  Serial.println();
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  Serial.print("Connecting to "); Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());

  // Connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // Set up a message handler for the button feed, will call
  // buttonPressed function whenever the dashboard button is pressed
  rgbLED->onMessage(setRGB);
  
  // Wait for a connection to Adafruit, print a "." every
  // half second while waiting
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // Print that Adafruit connection has been established
  Serial.println();
  Serial.println(io.statusText());

  // set analogWrite range for ESP8266
  #ifdef ESP8266
    analogWriteRange(255);
  #endif

  // Initialize ADPS9960
  if(!apds.begin()){
    Serial.println("failed to initialize ADPS9960! Please check your wiring.");
  }
  else Serial.println("ADPS9960 initialized!");

  // Gesture mode will be entered once proximity mode senses something close
  apds.enableProximity(true);
  apds.enableGesture(true);

  getGeo();

  Serial.println("Your external IP address is " + location.ip);
  Serial.print("Your ESP is currently in " + location.cn + " (" + location.cc + "),");
  Serial.println(" in or near " + location.cy + ", " + location.rc + ".");
  Serial.println("and located at (roughly) ");
  Serial.println(location.lt + " latitude by " + location.ln + " longitude.");
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();
  
  // If we put getIP() here, it would ping the endpoint over and over.
  uint8_t gesture = apds.readGesture();
  // Update location data if we detect an up or down gesture.
  if(gesture == APDS9960_UP || gesture == APDS9960_DOWN) {
    getGeo();
  }
}

// Return IP address for ESP8266 board.
String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json");
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      StaticJsonDocument<100> doc;
      String payload = theClient.getString();
      //   JsonObject& root = jsonBuffer.parse(payload);
      deserializeJson(doc, payload);
      ipAddress = doc["ip"].as<String>();

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  Serial.println(ipAddress);
  return ipAddress;
}

// Get the geolocation from ipstack API and deserialize the response.
void getGeo() {
  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + key); //return IP as .json object
// http://api.ipstack.com/97.113.62.196?access_key=...
  
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
 //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      StaticJsonDocument<1024> doc;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) {
        Serial.print("deserializeJson() failed with error code ");
        Serial.println(error.c_str());
        Serial.println(payload);
        return;
      }

      //Using .dot syntax, we refer to the variable "location" which is of
      //type GeoData, and place our data into the data structure.
      location.ip = doc["ip"].as<String>();            //we cast the values as Strings because
      location.cc = doc["country_code"].as<String>();  //the 'slots' in GeoData are Strings
      location.cn = doc["country_name"].as<String>();
      location.rc = doc["region_code"].as<String>();
      location.rn = doc["region_name"].as<String>();
      location.cy = doc["city"].as<String>();
      location.lt = doc["latitude"].as<String>();
      location.ln = doc["longitude"].as<String>();

      // Save the latitude and longitude to Adafruit IO feed.
      geolocation->save("Latitude: " + location.lt + " Longitude: " + location.ln);
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}

// Set RGB LED to color from Adafruit IO color picker data.
void setRGB(AdafruitIO_Data *data) { 
  // Pull RGB values from data and write to corresponding pins.
  analogWrite(RED_PIN, data->toRed());
  analogWrite(GREEN_PIN, data->toGreen());
  analogWrite(BLUE_PIN, data->toBlue());
}
