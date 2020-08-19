"""
This code creates a Twitter bot that posts temperature visualizations to the account @sea_antifreeze. 
In response to a daily MQTT message sent from a companion ESP8266 microcontroller, a graph is posted 
to Twitter displaying the high temperatures in Seattle for the current date since 1979. This code also 
publishes that data to an MQTT feed. Though the code is specific to Seattle, it was designed to be 
adaptable for any other location. The companion ESP8266 code automatically detects the location of that
device and publishes it to the location feed referenced in this code. The historical data was obtained 
through a bulk download (costing $10 USD) from OpenWeather. Anyone willing to acquire and clean that 
data can modify this code for their location.

This project uses the OpenWeather API (https://openweathermap.org/api)
and the Twitter Developer API (https://developer.twitter.com/en/docs/basics/getting-started).

Original MQTT Subscription client code by Thomas Varnish (https://github.com/tvarnish).

Written by Emma Meersman (https://github.com/emeersman)
"""

import json
import os
import paho.mqtt.client as mqtt
import tweepy

from datetime import datetime, timedelta

import math
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

# Variables for the MQTT broker
mqtt_username = "shoofly"
mqtt_password = "molassesflood2k19"
mqtt_broker_ip = "192.168.1.42"

# MQTT feed names
locationFeed = "location"  # updates when ESP8266 starts up
historicalFeed = "weather/history" # used to send historical weather data
dailyFeed = "weather/daily" # communicates daily high temp and weather for a location
queryFeed = "weather/query" # query for historical weather data

# Individual developer information for the Twitter API
api_key = "mBm2v1yGuRomHGBsv0xAhl5Jd"
api_key_secret = "06VCzoTISAXGcvggpOrNY4aFluRWP9s9keQl0gLVbnb4egp3an"
access_token = "1295172008108408832-F8GJfT0z47Pl0an9ZnpwAnG3WE6UDH"
access_token_secret = "3TN6vloONtzwrJOtqaqkSNyOFruMarYUCV7kziddywz4t"

# Set the username and password for the MQTT client
client = mqtt.Client()
client.username_pw_set(mqtt_username, mqtt_password)

# Authenticate for the Twitter API
auth = tweepy.OAuthHandler(api_key, api_key_secret)
auth.set_access_token(access_token, access_token_secret)
twitterAPI = tweepy.API(auth)

# Get current date
date = datetime.now().date()

# Variable tracking the name of the location for which to get the current weather
location = ""

# File name of the historical plot for the current date
dailyPlotName = ""

# Daily high temperature at the current location
temperature = 0.0

# High temperature for the previous day at the current location
prevTemperature = 0.0

# Track historical high/low temps (and associated years) for any date
historicalHighTemp = 0.0
historicalHighYear = ""
historicalLowTemp = 100.0
historicalLowYear = ""

# Handles what happens when the MQTT client connects to the broker
def on_connect(client, userdata, flags, rc):
    # rc is the error code returned when connecting to the broker
    print("Connected!", str(rc))
    
    # Once the client has connected to the broker, subscribe to the topic
    client.subscribe(locationFeed)
    client.subscribe(dailyFeed)
    client.subscribe(queryFeed)

    
# Handles what happens when the MQTT client receives a message
def on_message(client, userdata, msg):
    print("Topic: ", msg.topic + "\nMessage: " + str(msg.payload))
    if (msg.topic == locationFeed):
        deserializedJson = json.loads(msg.payload)
        # Save location name (of ESP8266)
        global location
        location = deserializedJson["city_name"]
    elif (msg.topic == dailyFeed):
        deserializedJson = json.loads(msg.payload)
        
        # Receive current/former day temperatures
        global temperature
        temperature = deserializedJson["temperature"]
        global prevTemperature
        prevTemperature = deserializedJson["prev_temperature"]

        # Update date variable
        date = datetime.now().date()

        # Update historical temperature data once per day with previous day high temperature 
        update_historical_file()
        publish_historical_data(True)
    elif (msg.topic == queryFeed):
        # TODO(future): modify this so it can accept any date
        date = datetime.now().date()
        publish_historical_data()


# Reads and outputs historical temperature data for the current date. Always publishes
# to an MQTT feed and optionally posts a graph of the data to Twitter.
def publish_historical_data(shouldPostData = False):
    historicalData = get_historical_data()        
    serializedData = json.dumps({"historical": historicalData})
    client.publish(historicalFeed, serializedData, 0, True) 
    if (shouldPostData):
        graph_historical_data(historicalData)
        tweet_historical_data()


# Appends the temperature data for the previous day to the JSON file so that 
# the file contains updated data. Writes the temperature for the previous day
# so that the information is based on past data instead of the current day forecast.
def update_historical_file():
    # Remove curly brace at the end of the file
    with open('/home/shoofly/Documents/seattle_data.json', 'rb+') as f:
        f.seek(-1, os.SEEK_END)
        f.truncate()
    # Write the previuos date and temperature to the end of the file
    with open('/home/shoofly/Documents/seattle_data.json', 'a') as g:
        prevDate = (datetime.now() - timedelta(1)).strftime('%Y-%m-%d')
        g.write(", \"" + prevDate + "\": " + str(prevTemperature) + "}")


# Gets historical weather data in a given city for the current date over the past 40 years (1979-2020).
# Historical data for any city is available through a bulk download from OpenWeather.
# The data should be transformed and saved as "{city_name}_data.json"
def get_historical_data():
    dateStr = str(date)
    historicalArray = []
    # Get all historical temperatures since 1979
    with open('/home/shoofly/Documents/seattle_data.json') as f:
        data = json.load(f)
    # Pull out each temperature matching the current month/day
    for key, value in data.items():
        if (key[5:] == dateStr[5:]):
            historicalArray.append({"date": key, "temp": value})
            # Track historical high and low temperatures for the current day
            if (value > historicalHighTemp):
                global historicalHighTemp
                historicalHighTemp = value
                global historicalHighYear
                historicalHighYear = key[0:4]
            elif (value < historicalLowTemp):
                global historicalLowTemp
                historicalLowTemp = value
                global historicalLowYear
                historicalLowYear = key[0:4]
    # Append the forecast for the current day to this array so it will be published to the 
    # MQTT feed and appear in the graph
    historicalArray.append({"date": dateStr, "temp": temperature})
    return historicalArray


# Generate a graph of the historical data for the current date. The graph
# displays a bar for each year colored according to the temperature on that day.
def graph_historical_data(data):
    y = []
    yColor = []
    # Capture historical temperatures in an array
    for entry in data:
        value = entry["temp"]
        y.append(value)
        # Assign each bar a color from blue to red based on the temperature value along the given scale
        yColor.append(strRgb(value, 20, 100))

    # TODO (future): modify code to post warming stripes 
    # use actual warming stripes code instead of just the coolwarm range
    # img = plt.imshow([y, y2], cmap='coolwarm')

    # Plot the data in a bar graph
    x = range(1979, date.year+1, 1)
    y2 = np.full(date.year+1-1979, y)
    plt.bar(x, y2, width=1.0, color=yColor)
    plt.gca().set_axis_off()
    plt.autoscale(tight=True)

    # Save the graph with a unique, descriptive name
    global dailyPlotName
    dailyPlotName = "/home/shoofly/Documents/" + str(date) + "_SeattleHistoricalPlot.png"
    plt.savefig(dailyPlotName, bbox_inches = 'tight', pad_inches = 0)


# Posts the historical data plot to Twitter with a descriptive caption.
def tweet_historical_data():
    startingYear = "1979"
    tweetMessage = "Temperature data for " + str(date.month) + "/" + str(date.day) + " in #" + location + " from " + startingYear + " - " + str(date.year) + ". Forecast high for today is " + str(temperature) + "\u00B0F. Historical temps range from " + str(historicalLowTemp) + "\u00B0F (" + historicalLowYear + ") to " + str(historicalHighTemp) + "\u00B0F (" + historicalHighYear + ") #showyourstripes"

    # Upload daily historical plot to API
    media_list = []
    response = twitterAPI.media_upload(dailyPlotName)
    media_list.append(response.media_id)

    # Post historical plot and message to Twitter
    twitterAPI.update_status(status=tweetMessage, media_ids=media_list)


# The three functions below convert a numerical value along a given scale to a string hex value
# ranging from blue (low/cold) to red (high/hot).
# Sourced from https://learning.oreilly.com/library/view/python-cookbook/0596001673/ch09s11.html
# Modified RGB equations in floatRGB
def floatRgb(mag, cmin, cmax):
    """ Return a tuple of floats between 0 and 1 for R, G, and B. """
    # Normalize to 0-1
    try: x = float(mag-cmin)/(cmax-cmin)
    except ZeroDivisionError: x = 0.5 # cmax == cmin
    if (x <= 0.5):
        green = min((max((2.*x, 0.)), 1.))
    else:
        green  = min((max((2.*(0.9-x), 0.)), 1.))
    red   = min((max((4*(x-0.25), 0.)), 1.))
    blue = min((max((4*math.fabs(x-0.75)-1., 0.)), 1.))
    return red, green, blue

def rgb(mag, cmin, cmax):
    """ Return a tuple of integers, as used in AWT/Java plots. """
    red, green, blue = floatRgb(mag, cmin, cmax)
    return int(red*255), int(green*255), int(blue*255)

def strRgb(mag, cmin, cmax):
    """ Return a hex string, as used in Tk plots. """
    return "#%02x%02x%02x" % rgb(mag, cmin, cmax)


# Tell the client which functions are to be run on connecting and on receiving a message
client.on_connect = on_connect
client.on_message = on_message

# Once everything has been set up, we can (finally) connect to the broker
# 1883 is the listener port that the MQTT broker is using
client.connect(mqtt_broker_ip, 1883)

# Once we have told the client to connect, let the client object run itself
client.loop_forever()
client.disconnect()