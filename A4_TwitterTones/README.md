# Twitter Tones

### Overview

This project was an open-ended challenge to create something that would pull data from two different sources and display that data. I created a comparative visual representation of sentiment from tweets about two topics. Using a Raspberry Pi as an MQTT broker, I set up an MQTT feed to communicate two query strings between clients and the broker. I created a Python program on the Raspberry Pi to subscribe to the query feed. As an aside, I made a service for this program on the Raspberry Pi so that it would begin running on startup. In this program, I used the Twitter API to obtain recent tweets about each query, which I then fed into the IBM Watson Tone Analyzer API.

### Sentiment Analysis

This API uses natural language processing to classify text according to sentiments. For example, the sentence, “I’m so happy right now!” would be classified as joyful. The API can assign more than one sentiment to text. A response from the API includes a list of sentiments coupled with the Tone Analyzer’s confidence (value between 0.5 and 1) that the given text displays that emotion. I normalized each of the confidence values so that they could be processed like percentages (i.e. the sentiment with the highest confidence value would correspond to the highest percentage for a query). This transformation of data is not accurate to the meaning of the confidence values but it worked well for translating the sentiments to a visual representation.

### Hardware

Once I performed sentiment analysis on each of the two queries, I published the resulting sentiment lists to an MQTT feed. My Arduino code was subscribed to this feed along with the query feed. I mapped each possible sentiment to an RGB color value and displayed the sentiment color of each query on an RGB LED. If the tweets for a query yielded multiple sentiments, I calibrated the RGB value to be a combination of each sentiment color relative to the percentage of confidence returned from the API. For example, if the sentiment from a group of tweets was 40% angry (red) and 60% sad (blue), the LED would turn a bluish purple. In addition to the RGB LEDs, the breadboard held an OLED screen which displayed both queries.

### Future Work

There are many opportunities for future work on this project. My Arduino code currently publishes to the query feed every 30 seconds to run the entire process and update the LEDs with the newest batch of tweets. This could easily be moved into the Python code as part of the call to the Twitter API. I chose to keep it in the Arduino code since we have focused on MQTT pub/sub from Arduino in this class. Also, I believe that this project would greatly benefit from a more attractive enclosure. When I have more time, I want to 3D print two hollow birds from white filament and use them as covers for the LEDs. One of the main reasons I chose to do this project was to experiment with APIs I found interesting. Given that I intend to explore the Tone Analyzer API further in future projects, I would call this a successful experiment.
