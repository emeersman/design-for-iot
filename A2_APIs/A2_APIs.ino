/*
 * Parts of this code were sourced from in-class example code here:
 * https://gist.github.com/brockcraft/6cc4e19a1bb557abcf3f034a677a8945
 * 
 * The ESP8266 HTTPClient examples on Github were also helpful:
 * https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPClient/examples
 * 
 * This code makes calls to three different APIs:
 * jService (Jeopardy questions): https://jservice.io/
 * XKCD (popular webcomic): https://xkcd.com/json.html
 * ISS Location: http://open-notify.org/Open-Notify-API/ISS-Location-Now/
 * 
 */

// Define this to parse special characters from JSON responses
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h> //provides the ability to parse and construct JSON objects

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

//Configuration parameters
const char* ssid = "";
const char* pass = "";

// Current trivia question answer, initialized to a random string.
String currAnswer = "thisisnotagoodanswer";

void setup() {
  // Begin the serial monitor
  Serial.begin(115200);
  delay(10);

  // Print the name of the network we are trying to connect to
  Serial.print("Connecting to "); Serial.println(ssid);

  // Start connecting to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  // Print a loading message until the WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print when the WiFi is connected along with the board IP address
  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());

  // Beginning of the serial monitor game
  Serial.println("Enter a number between 1 and 2335 to get a comic and a trivia question");
  Serial.println();
}

void loop() {
  // Check if data has been received from serial monitor input
  if (Serial.available() > 0) {
    // Read the incoming serial monitor string
    String serialString = Serial.readString();
    
    // Convert the input string to lower case and trim excess whitespace
    serialString.toLowerCase();
    serialString.trim();

    // Check whether the current input was the correct answer to the most recently asked trivia question
    if (currAnswer.indexOf(serialString) > -1) {
      // Call ISS API if the user answers the trivia question correctly
      Serial.println("Correct! Here's a cool fact: ");
      getISS();
      Serial.println();
    } else {
      // Read comic number as input from serial monitor
      String comicNumber = String(serialString.toInt());

      // Checks whether the input is a valid number and fetches the 
      // corresponding comic, otherwise fetches a random comic.
      if (comicNumber != "0") {
        Serial.println("Fetching comic number " + comicNumber);
      } else {
        Serial.println("Fetching random comic");
        comicNumber = "";
      }
      // Call the XCKD and Jeopardy APIs
      getXkcd(comicNumber);
      Serial.println();
      getJeopardy();
      Serial.println();
    }
  }
}

// Call the XKCD API to get data on the specified comic number.
void getXkcd(String comicNumber) {
  // Create an http client and connect to the XKCD endpoint
  HTTPClient httpClient;
  const char* fingerprint ="7A 63 0B 5F F6 72 E8 4D 70 B7 8B 45 1D CF 27 94 AF 2C F1 9A";
  httpClient.begin("https://xkcd.com/" + comicNumber + "/info.0.json", fingerprint);

  // Request the information from the webserver
  int httpCode = httpClient.GET();

  // Ensure that the response was good, otherwise print an error
  if(httpCode == HTTP_CODE_OK) {
    // Create a dynamic document to hold the deserialized response
    DynamicJsonDocument doc(2048);

    // Get the response from the client and print it out
    String response = httpClient.getString();
    Serial.println(response);

    // Deserialize the JSON response and throw an error if necessary
    DeserializationError error = deserializeJson(doc, response);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with error code ");
      Serial.println(error.c_str());
      return;
    }

    // Parse the comic name and image from the response and print them
    String comicName = doc["safe_title"].as<String>();
    String imgLink = doc["img"].as<String>();
    Serial.println("Comic name: " + comicName);
    Serial.println("Link to comic: " + imgLink);
  } else {
    Serial.println("Error connecting to XKCD endpoint.");
  }

  // Close the http connection
  httpClient.end();
}

void getJeopardy() {
  //Creates a Http handler
  HTTPClient jClient;

  const char* jFingerprint = "6B 87 88 19 2E 5E 9E C1 3F 6F 38 93 E9 7F 16 EC FE 0A 57 F2";
  // Make the Https Request, using the fingerprint provided at the top of the sketch
  jClient.begin("https://jservice.io/api/random", jFingerprint);
  
  // Request the information from the webserver
  int httpCode = jClient.GET();

  // Ensure that the response was good, otherwise print an error
  if(httpCode == HTTP_CODE_OK) {
    // Create a dynamic document to hold the deserialized response
    DynamicJsonDocument doc(2048);

    // Get the response from the client and print it out
    String response = httpClient.getString();
    Serial.println(response);

    // Deserialize the JSON response and throw an error if necessary
    DeserializationError error = deserializeJson(doc, response);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with error code ");
      Serial.println(error.c_str());
      return;
    }
    // Parse the question and answer from the response
    String question = doc[0]["question"].as<String>();
    String answer = doc[0]["answer"].as<String>();

    // Print the question and save the answer as the global variable
    Serial.println("Question: " + question);
    currAnswer = answer;
    currAnswer.toLowerCase();
  } else {
    Serial.println("Error connecting to Jeopardy endpoint.");
  }

  // Close the http connection
  jClient.end();
}

// Call the ISS API to get data on the current location of the ISS.
void getISS() {
  // Create an http client and connect to ISS endpoint
  HTTPClient tClient;
  tClient.begin("http://api.open-notify.org/iss-now.json");

  // Request the information from the webserver
  int httpCode = tClient.GET();

  // Ensure that the response was good, otherwise print an error
  if(httpCode == HTTP_CODE_OK) {
    // Create a static document to hold the deserialized response
    StaticJsonDocument<255> doc;

    // Get the response from the client and print it out
    String response = httpClient.getString();
    Serial.println(response);

    // Deserialize the JSON response and throw an error if necessary
    DeserializationError error = deserializeJson(doc, response);
    // Test if parsing succeeds.
    if (error) {
      Serial.print("deserializeJson() failed with error code ");
      Serial.println(error.c_str());
      Serial.println(response);
      return;
    }
    // Parse the current ISS lat and long from the deserialized response and print them
    String issLat = doc["iss_position"]["latitude"].as<String>();
    String issLong = doc["iss_position"]["longitude"].as<String>();
    Serial.println("The ISS is currently located at " + issLat + " latitude and " + issLong + " longitude.");
  } else {
    Serial.println("Error connecting to ISS endpoint.");
  }

  // Close the http connection
  tClient.end();
}
