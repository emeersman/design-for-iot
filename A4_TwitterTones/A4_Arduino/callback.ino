// Listen for incoming messages from the broker on feeds defined in config.h

// MQTT server will always send back a char and byte payload, and the total length
void callback(char* topic, byte* payload, unsigned int length) {
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

  // Copy incoming query data to local variables then update OLED display if either query changed
  if (strcmp(topic, twitterQueryFeed) == 0) {
    String currFirst = doc["q1"].as<String>();
    String currSecond = doc["q2"].as<String>();
    // Check whether either of the queries is different from the saved ones
    if (!currFirst.equals(firstTwitterQuery) || !currSecond.equals(secondTwitterQuery)) {
      firstTwitterQuery = currFirst;
      secondTwitterQuery = currSecond;
      drawText();
    }
  } else if (strcmp(topic, twitterResponseFeed) == 0) { // Process sentiment response
    // Initialize two RGB colors corresponding to both RGB LEDs
    RGB colors[2] = {{0, 0, 0}, {0, 0, 0}};
    int topicNum = 0;
    JsonArray jsonArray = doc.as<JsonArray>();
    // Loop through each query topic in the response
    for (JsonObject topicObject : jsonArray) {
      String topic = topicObject["topic"].as<String>();
      // Get the JSON object corresponding to each emotion name and percent
      JsonObject emotions = topicObject["emotions"].as<JsonObject>();
      // Loop through each emotion object in the response
      for (JsonPair emotion : emotions) {
        // Use the emotion name to get the corresponding color in the sentiment dictionary
        String emotionStr = emotion.key().c_str();
        char emotionChar = emotionStr.equals("analytical") ? 'l' : emotionStr[0];
        RGB emotionColor = getEmotionColor(emotionChar);
        // Get the corresponding percentage for the emotion
        float emotionPct = emotion.value().as<float>();

        // Add the color values for the emotion to the final LED color value. Multiply
        // by the emotion percent to represent each emotion accordingly in the final color.
        colors[topicNum].red += (int)(emotionPct * emotionColor.red);
        colors[topicNum].green += (int)(emotionPct * emotionColor.green);
        colors[topicNum].blue += (int)(emotionPct * emotionColor.blue);
      }
      topicNum += 1;
    }
    // Update the RGB LED colors with the sentiments, confirm that we received a response
    setRGB(colors[0], false);
    setRGB(colors[1], true);
    receivedResponse = true;
  } 
}

// Given an emotion, return the corresponding color from the sentiment dictionary.
RGB getEmotionColor(char emotion) {
  RGB emotionColor = {0, 0, 0};
  switch(emotion) {
    case 'a':
      emotionColor = sentimentDict.anger;
      break;
    case 'f':
      emotionColor = sentimentDict.fear;
      break;
    case 'j':
      emotionColor = sentimentDict.joy;
      break;
    case 's':
      emotionColor = sentimentDict.sadness;
      break;
    case 'l':
      emotionColor = sentimentDict.analytical;
      break;
    case 'c':
      emotionColor = sentimentDict.confident;
      break;
    case 't':
      emotionColor = sentimentDict.tenative;
      break;
    case 'n':
      emotionColor = sentimentDict.neutral;
      break;
    default:
      break;
  }
  return emotionColor;
}
