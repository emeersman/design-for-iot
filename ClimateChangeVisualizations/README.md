# Climate Change Visualizations

## Introduction

This project is a system of visualizing historical climate change across multiple platforms: an LED strip and graphs posted by a Twitter bot. It is intended to increase awareness and provide knowledge about rapidly rising temperatures around the world over the past few decades. It provides both physical and digital visualizations in order to effectively reach wider audiences. These visualizations were inspired by [existing climate data tools](https://showyourstripes.info/) developed by people like Ed Hawkins. This project also incorporates ambient displays as tools for displaying climate data through unexpected and potentially impactful experiences. The system was also designed to be adaptable for any location around the world to bring personally meaningful data on climate change to as many people as possible.

## System Architecture

The system itself consists of several pieces: location-based data, an MQTT network (in this case running on a Raspberry Pi), a Python client, a microcontroller client (here an ESP8266 HUZZAH), API integration, and hardware components. Each of these will be explored in more detail below.


### Location-based Data

Unexpectedly, it was quite difficult to find consistent, free historical weather data with any degree of granularity. Many sources have historical temperature data at a very large scale (ex. country-level) or data that doesn’t span many years. This data is available through several APIs at exorbitant prices, often with an enterprise-level subscription. The data used in this project was obtained from the [OpenWeather History Bulk](https://openweathermap.org/history-bulk) download tool. For the price of $10 USD per location, OpenWeather provides a bulk file of hourly temperature data stretching back several decades. The data for Seattle, used in this project, dated back to January 1, 1979. Anyone wishing to replicate this project would need to pay the OpenWeather fee for their desired location.

Understandably, 40+ years of hourly weather data is not small. This project focuses on daily high temperatures, though the code could be modified to analyze any property of this weather data (ex. precipitation). The python script city_data_parser.py extracts the high temperature data from each hourly object and determines the maximum temperature for each day. It then saves the date and temperature in a new file. This file is stored on a Raspberry Pi and read/modified by the python MQTT client.

### MQTT Network
The backbone of this project was an MQTT network running on mosquitto, which had been installed on a Raspberry Pi. The network supported a total of two clients and four topics. These topics allowed locations, temperature data, and historical weather to pass between the python client on the Pi and the microcontroller client. The broker was initialized in PiClientCode.py. In order to ensure that the broker does not get disconnected, it was helpful to configure that file as a service that would run when the Pi boots up.
Python Client
This client handles the historical data manipulation logic and sends the historical data for a given day to multiple sources. The client subscribes to a location feed and uses that information to customize the status message posted to Twitter.

The client also subscribes to a daily feed. This feed posts the high temperatures for the current and previous day. In response to a message from that topic, the client writes the previous day’s data to the historical JSON data file. The bulk data download from OpenWeather only includes data until the day it is downloaded. This logic ensures that the data file is kept current. The client then reads all historical temperatures for the current day and month and combines them in a JSON array with the current temperature data. This array is then published to a historical feed (to be received by the ESP8266 client) and used to generate a graph. The graph is then posted to Twitter along with contextual information.

This project involved creating a Twitter account ([@sea_antifreeze](https://twitter.com/sea_antifreeze)). It integrates with the Twitter Developer API to post to a specific account determined by the access credentials at the top of the file. Another implementation of this project (theoretically for a different location) would require setting up a unique Twitter account with a separate set of credentials.

Finally, the client subscribes to a query feed that obtains the historical data for the current date without writing to the JSON file or posting to Twitter.

      
### Microcontroller Client

The logic on the ESP8266 is responsible for calling several APIs, interacting with hardware components, and publishing time-based messages to MQTT feeds. It uses timing logic to publish the current and previous temperatures at the beginning of every day. It also publishes the current location of the device whenever it reboots. All of this logic is contained within the ArduinoCode.ino and callback.ino files. The config.h file should contain all necessary information to connect to WiFi and the MQTT broker. It should also contain several API keys.

### API Integration

The Arduino code calls the [ipify](https://www.ipify.org/) and [ipstack](https://ipstack.com/) APIs to get the geolocation of the ESP8266. The city name and latitude/longitude coordinates are published to the location feed and called by subsequent APIs, respectively. In addition to the historical bulk download, the [OpenWeather OneCall API](https://openweathermap.org/api/one-call-api) is used in this project to get the high temperatures for the current and previous days. The API provides hourly temperature data for a given day in the past week. This API needs to be called several times to get all temperatures for the previous day once time zone offsets are considered. 

The [OpenWeather OneCall API](https://openweathermap.org/api/one-call-api) is also used to get daily forecast data for the next week. The LED strip supports a mode to display the weekly forecast.

### Hardware Components

This project supports a button and photoresistor as sources of physical input. It supports an OLED display and LED strip for physical output. The button allows users to cycle through display modes. These display modes consist of an off mode, a daily forecast mode, and a historical data mode. The photoresistor is used to implement adaptive brightness for the LED strip, meaning that the strip will shine brighter when more ambient light is detected and dimmer when there is less light.

The OLED displays the name of the location and specific data about the mode. In the daily forecast mode, the OLED shows the current temperature in the given location as well as the weekly high and low forecasted temperatures. In historical mode, the OLED shows the temperature forecast for the current day as well as the historical high and low temperatures. It also shows the relevant years.

Finally, the LED strip displays the high temperatures for the given mode converted to RGB values. The same algorithm is used to convert temperature to RGB for both the Twitter charts and the LED values. In daily mode, the pixels on the strip are divided into equal chunks, each of which shows the forecast temperature color for a given day. In historical mode, each pixel is mapped to a past year. 
