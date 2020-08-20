import ijson
import json

f = open('ow_full_seattle_dataset.json', 'r')
hourly_weather_items = ijson.kvitems(f, 'item')

main = (v for k, v in hourly_weather_items if k == 'main')

max_temps = []
count = 0
for val in main:
    temps = []
    currTemp = float(val['temp_max'])
    temps.append(currTemp)
    count += 1
    if (count == 24):
        max_temps.append(max(temps))
        count = 0

f.seek(0)
date_items = ijson.kvitems(f, 'item')

dates = (v for k, v in date_items if k == 'dt_iso')

date_list = []
for date in dates:
    date_list.append(date[0:10])

dates = list(dict.fromkeys(date_list))

f.close()

output = open('parsed_historical_data.json', 'w')
output.write(json.dumps(dict(zip(dates, max_temps))))
output.close()