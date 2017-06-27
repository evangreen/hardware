##
## Icegrid weather forecast.
##
## Copyright (c) 2017 Evan Green. All Rights Reserved.
##

import json
import socket
import sys
import time
from urllib import urlopen
from urllib import quote

# Address of the IceGrid.
UDP_IP = "192.168.1.147"
UDP_PORT = 8080

# My crappy developer key.
API_KEY = "2e2103ffbc917429"
LOCATIONS = ["CA/Menlo_Park",
             "CA/Tahoma",
             "NY/Paul_Smiths"]

icon_to_code = {
    'chanceflurries': 'chancesnow',
    'chancerain': 'chancerain',
    'chancesleet': 'chancesnow',
    'chancesnow': 'chancesnow',
    'chancetstorms': 'chancerain',
    'flurries': 'snow',
    'sleet': 'snow',
    'snow': 'snow',
    'rain': 'rain',
    'tstorms': 'rain'
}

code_to_color = {
    "blank": "000000",
    "chancerain": "004000",
    "chancesnow": "008040",
    "rain": "30FF00",
    "snow": "00FFFF",
    "0": "FF8000",
    "10": "0000FF",
    "30": "3008FF",
    "40": "6000C0",
    "50": "A00090",
    "60": "C010C0",
    "70": "C080A0",
    "80": "E03040",
    "90": "FF0020",
    "100": "FF0000",
    "110": "FF3000",
    "unknown": "FFFFFF"
}

def location_to_path(location):
    location = location.replace('/', '_')
    return location + ".json"

def fetch_data(location):
    req = ("http://api.wunderground.com/api/%s/conditions/forecast10day/q/"
           "%s.json" % (API_KEY, quote(location)))

    print(req)
    try:
        response = urlopen(req)

    except Exception as e:
        if hasattr(e, 'reason'):
            print(e.reason)

        elif hasattr(e, 'code'):
            print("Status returned: " + str(e.code))

        else:
            print(e)

        return None

    json_data = response.read().decode('utf-8', 'replace')
    data = json.loads(json_data)
    try:
        print(data['response']['error']['description'])
        return None

    except KeyError:
        pass

    f = open(location_to_path(location), "w")
    f.write(json_data)
    f.close()
    return data

def convert_to_code(day):
    try:
        code = icon_to_code[day['icon']]
        return code

    except KeyError:
        pass

    high = int(day['high']['fahrenheit'])
    if high <= 0:
        return "0"

    elif high < 15:
        return "10"

    elif high <= 30:
        return "30"

    elif high <= 45:
        return "40"

    elif high <= 55:
        return "50"

    elif high <= 65:
        return "60"

    elif high <= 75:
        return "70"

    elif high <= 85:
        return "80"

    elif high <= 95:
        return "90"

    elif high < 105:
        return "100"

    return "110"

def convert_to_color(code):
    try:
        return code_to_color[code]

    except KeyError:
        return code_to_color["unknown"]

def get_forecast(location):
    data = fetch_data(location)
    #f = open(location_to_path(location), 'r')
    #data = json.loads(f.read())
    #f.close()
    simple_forecast = data['forecast']['simpleforecast']['forecastday']
    codes = []
    for day in range(0, 5):
        codes.append(convert_to_code(simple_forecast[day]))

    print("%s: %s" % (location, codes))
    return codes

def get_all_codes():
    codes = []
    for location in LOCATIONS:
        codes += get_forecast(location)

    return codes

def get_led_string(codes):
    values = []
    for code in codes:
        values.append(convert_to_color(code))

    return ','.join(values)

def update_leds():
    if time.localtime().tm_hour < 7:
        string = "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"

    else:
        codes = get_all_codes()
        string = get_led_string(codes)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
    sock.sendto(string + "\r\n", (UDP_IP, UDP_PORT))
    sock.close()

while True:
    print(time.strftime('%Y-%m-%d %H:%M:%S', time.localtime()))
    update_leds()
    time.sleep(3600)
