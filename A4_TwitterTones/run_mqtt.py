"""
Create a Twitter sentiment analysis platform that performs sentiment analysis on tweets about 
two topics and maps those sentiment values to RGB LEDs. The topics are displayed on an OLED screen.
The topics and sentiment analyses are published to MQTT feeds. Any subscriber could change the
current query topics and/or receive the sentiment data.

This project invokes the Watson Tone Analyzer API (https://www.ibm.com/watson/services/tone-analyzer/)
and the Twitter Developer API (https://developer.twitter.com/en/docs/basics/getting-started).

This file was designed to run continuously on a Raspberry Pi acting as an MQTT broker.

Original MQTT Subscription client code by Thomas Varnish (https://github.com/tvarnish).

Written by Emma Meersman (https://github.com/emeersman)
"""

import json
import paho.mqtt.client as mqtt
import tone_analyzer
import tweepy

# Variables for the MQTT broker
mqtt_username = ""
mqtt_password = ""
mqtt_broker_ip = ""
mqtt_topic = "twitter/query"
sentiment_topic = "twitter/sentiment"

# Individual developer information for the Twitter API
api_key = ""
api_key_secret = ""
access_token = ""
access_token_secret = ""

# Set the username and password for the MQTT client
client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)

# Authenticate for the Twitter API
auth = tweepy.OAuthHandler(api_key, api_key_secret)
auth.set_access_token(access_token, access_token_secret)
twitterAPI = tweepy.API(auth)

# Variable tracking number of tweets to be fetched in one call to the API
numFetchedTweets = 10

# Handles what happens when the MQTT client connects to the broker
def on_connect(client, userdata, flags, rc):
    # rc is the error code returned when connecting to the broker
    print("Connected!", str(rc))
    
    # Once the client has connected to the broker, subscribe to the topic
    client.subscribe(mqtt_topic)

    
# Handles what happens when the MQTT client receives a message
def on_message(client, userdata, msg):
    print("Topic: ", msg.topic + "\nMessage: " + str(msg.payload))
    if(msg.topic == mqtt_topic):
        analyze_topics(msg)

# Gets Twitter topics from payload, calls the function to perform sentiment
# analysis, and publishes the analyzed text to an MQTT feed.
def analyze_topics(msg):
    # Deserialize JSON and extract the Twitter query topics
    deserializedJson = json.loads(msg.payload)
    firstQuery = deserializedJson["q1"]
    secondQuery = deserializedJson["q2"]

    # Perform sentiment analysis on the text from each query and append the
    # results to an array
    sentimentPayload = []
    sentimentPayload.append(analyze_sentiment(firstQuery))
    sentimentPayload.append(analyze_sentiment(secondQuery))

    # Serialize the results of the sentiment analysis and publish to an MQTT feed
    serializedPayload = json.dumps(sentimentPayload)
    print(serializedPayload)
    client.publish(sentiment_topic, serializedPayload)


# Fetch combined tweet text and perform sentiment analysis to get sentiments and 
# confidence percentages for each one.
def analyze_sentiment(topic): 
    # Get the text content of tweets for the given topic
    text = fetch_tweets(topic)
    toneArray = []
    toneDict = {}
    # Check that the tweets returned text, otherwise return empty toneDict
    if text:
        # Call tone analyzer function on tweets
        sentimentJson = tone_analyzer.analyze_tone(text)
        # Deserialize Watson API response
        sentiments = json.loads(sentimentJson)
        toneArray = sentiments["document_tone"]["tones"]
        # If the API couldn't identify any tones with enough confidence, assign
        # it to neutral
        if not toneArray:
            toneDict["neutral"] = 1.0
    # Add all confidence scores across tones
    totalScore = 0.0
    for tone in toneArray:
        totalScore += tone["score"]
    # Save each tone and normalized confidence percentage to the dictionary
    for tone in toneArray:
        toneId = tone["tone_id"]
        toneDict[toneId] = tone["score"] / totalScore
    return {"topic": topic, "emotions": toneDict}


# Call Twitter API to fetch the most recent tweets about a given query string
def fetch_tweets(query):
    if query:
        # Don't include retweets
        # To search hashtags, add a "#" to the beginning of the query string
        search_query = query.replace(" ", "+") + " -filter:retweets"
        tweets = tweepy.Cursor(twitterAPI.search,
                  q=search_query,
                  result_type="recent",
                  lang="en").items(numFetchedTweets)
        # Join all returned tweets into a single string of text
        return " ".join([tweet.text for tweet in tweets])
    return " "

# Here, we are telling the client which functions are to be run
# on connecting, and on receiving a message
client.on_connect = on_connect
client.on_message = on_message

# Once everything has been set up, we can (finally) connect to the broker
# 1883 is the listener port that the MQTT broker is using
client.connect(mqtt_broker_ip, 1883)

# Once we have told the client to connect, let the client object run itself
client.loop_forever()
client.disconnect()