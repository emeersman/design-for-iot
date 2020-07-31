// Listen for incoming messages from the broker on feeds defined in config.h

// MQTT server will always send back a char and byte payload, and the total length
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("--------------------");
  Serial.print("A message arrived from [");
  Serial.print(topic);
  Serial.println("] ");
  Serial.println();

  // Deserialize the incoming JSON, check for errors
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("deserializeJson() failed, are you sure this message is JSON formatted?");
    Serial.print("Error code: ");
    Serial.println(err.c_str());
    return;
  }

// Uncomment the lines below to print the incoming messages
//  Serial.println("The message was: ");
//  serializeJsonPretty(doc, Serial);
//  Serial.println();

  // Copy incoming message data to the corresponding property of the 
  // weather object, then update the OLED display with the new value
  if (strcmp(topic, indoorTempFeed) == 0) {
    weather.fahrenheit = doc["indoor_temp"]["fahrenheit"].as<float>();
    weather.celsius = doc["indoor_temp"]["celsius"].as<float>();
    drawText();
  } else if (strcmp(topic, outdoorTempFeed) == 0) {
    weather.outdoorFahrenheit = doc["outdoor_temp"]["fahrenheit"].as<float>();
    weather.outdoorCelsius = doc["outdoor_temp"]["celsius"].as<float>();
    String location = doc["location_name"].as<String>();
    strcpy(weather.location, location.c_str()); 
    drawText();
  } else if (strcmp(topic, humidityFeed) == 0) {
    weather.humidity = doc["humidity_pct"].as<float>();
    drawText();
  } else if (strcmp(topic, pressureFeed) == 0) {
    weather.pressure = doc["pressure_kpa"].as<float>();
    drawText();
  }

  // Not part of A3, prints whenever a button press is published on the lights feed
  if (strcmp(topic, "lights") == 0) {
    Serial.print("Button pressed: ");
    serializeJson(doc["buttonPressed"], Serial);
    Serial.println(); //give us some space on the serial monitor read out
  }
}
