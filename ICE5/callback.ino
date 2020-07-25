/////CALLBACK/////
//The callback is where we attacch a listener to the incoming messages from the server.
//By subscribing to a specific channel or topic, we can listen to those topics we wish to hear.
/////

// the standard format at least in this library is that the mqtt server will always send back a char and byte payload, and the total length
void callback(char* topic, byte* payload, unsigned int length) {

  Serial.println("--------------------");
  Serial.print("A message arrived from [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");
  Serial.println();

  drawText("Received message from " + String(topic) + " feed");

  // If you are having trouble, lines 16-18 will print out the payload received
  //for (int i = 0; i < length; i++) {
  //  Serial.print((char)payload[i]);
  //}

  StaticJsonDocument<256> doc;

  //Line 26 is the basic way to deserialize the message payload.
  //But it gives no error reporting if there is a failure.
  //deserializeJson(doc, payload, length);

  // A better way, with error reporting too!
  DeserializationError err = deserializeJson(doc, payload, length);

  if (err) { //well what was it?
    Serial.println("deserializeJson() failed, are you sure this message is JSON formatted?");
    Serial.print("Error code: ");
    Serial.println(err.c_str());
    return;
  }

  // For debugging or printing all messages
  //Serial.println("The message was: ");
  //serializeJsonPretty(doc, Serial);
  //Serial.println();

  if (strcmp(topic, "super/heroes") == 0) {
    Serial.print("Lego Batman Irony Level changed to: ");
    serializeJson(doc["Irony Level"], Serial);
    Serial.println();
  }

  else if (strcmp(topic, "super/star") == 0) {
    Serial.println("Weather report: ");
    Serial.print("Temperature: ");
    serializeJson(doc["temperature"], Serial);
    Serial.print(", Humidity: ");
    serializeJson(doc["humidity"], Serial);
    Serial.println();
  }

  else if (strcmp(topic, "super/mario") == 0) {
    Serial.print("The button state is being reported as: ");
    serializeJson(doc["button1"]["buttonState"], Serial);

    int temp = doc["button1"]["presses"].as<int>();  // get the value of button presses as an integer

    if (temp != 0) {   // if the vlaue is not zero, print it out.
      Serial.println();
      Serial.print("The button has been pressed ");
      Serial.print(temp);
      Serial.println(" times.");
    }

    //serializeJsonPretty(doc, Serial); //print out the parsed message: serializeJsonPretty();is a nice function we get from the JSON library
    Serial.println(); //give us some space on the serial monitor read out
  }
}
