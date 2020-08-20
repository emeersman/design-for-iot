// Set up WiFi network credentials and MQTT server credentials
// Define MQTT topics

#define wifi_ssid "" // YOUR network name
#define wifi_password "" // YOUR network password

#define mqtt_server "" // the MQTT server address
#define mqtt_user ""           // the user login
#define mqtt_password ""           // the password

#define locationFeed "location"
#define historicalFeed "weather/history"
#define dailyFeed "weather/daily"
#define queryFeed "weather/query"

// User-specific API keys
const String geolocation_key = "";
const String weather_api_key = "";
