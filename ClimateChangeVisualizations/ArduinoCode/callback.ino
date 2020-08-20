// Listen for incoming messages from the broker on feeds defined in config.h

// MQTT server will always send back a char and byte payload, and the total length
void callback(char* topic, byte* payload, unsigned int length) {
  // Deserialize the incoming JSON, check for errors
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("deserializeJson() failed, are you sure this message is JSON formatted?");
    Serial.print("Error code: ");
    Serial.println(err.c_str());
    return;
  }

  // Print the incoming messages
//  Serial.println("The message was: ");
//  serializeJsonPretty(doc, Serial);
//  Serial.println();

  // Save historical data to global variable, update the display if currently showing historical data
  if (strcmp(topic, historicalFeed) == 0) {
    JsonArray historicalObjects = doc["hist"].as<JsonArray>();
    int historicalCount = 0;
    for (int i = 0; i < 42; i++) {
      historicalArray[historicalCount] = historicalObjects[i].as<float>();
      historicalCount += 1;
    }
    if (currentMode == HISTORICAL) {
      updateDisplay();
    }
  }
}
