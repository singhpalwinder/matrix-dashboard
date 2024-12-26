#! /home/psingh/matrix-dashboard/venv/bin/python3
from rgbmatrix import RGBMatrix, RGBMatrixOptions
from PIL import Image, ImageDraw, ImageFont
from datetime import datetime
import time, config, json, math


# Full matrix dimensions
MATRIX_WIDTH = 64
MATRIX_HEIGHT = 32

# Section dimensions (example: clock area 38x8, date area 24x8)
CLOCK_WIDTH, CLOCK_HEIGHT = 38, 8
DATE_WIDTH, DATE_HEIGHT = 58, 8
WEATHER_WIDTH, WEATHER_HEIGHT = 58, 8

# Configuration options for the RGB Matrix
options = RGBMatrixOptions()
options.rows = 32
options.cols = 64
options.chain_length = 1
options.parallel = 1
options.brightness = 50
options.hardware_mapping = "adafruit-hat"

matrix = RGBMatrix(options=options)

# Load fonts
clock_font = ImageFont.truetype("/usr/share/fonts/MatrixChunky6.ttf", 6)
date_font = ImageFont.truetype("/usr/share/fonts/MatrixChunky6.ttf", 6)
weather_font = ImageFont.truetype("/usr/share/fonts/MatrixChunky6.ttf", 6)

# Create independent canvases
full_canvas = Image.new("RGB", (MATRIX_WIDTH, MATRIX_HEIGHT), "black")  # Full display canvas
clock_canvas = Image.new("RGB", (CLOCK_WIDTH, CLOCK_HEIGHT), "black")  # Clock canvas
date_canvas = Image.new("RGB", (DATE_WIDTH, DATE_HEIGHT), "black")  # Date canvas
weather_canvas = Image.new("RGB", (WEATHER_WIDTH, WEATHER_HEIGHT), "black")  # Weather canvas

# Create draw objects for each canvas
clock_draw = ImageDraw.Draw(clock_canvas)
date_draw = ImageDraw.Draw(date_canvas)
weather_draw = ImageDraw.Draw(weather_canvas)

# Positioning of each section on the full matrix
DATE_X, DATE_Y = 5, 2      # Position date at the top
CLOCK_X, CLOCK_Y = 15, 12  # Center the clock
WEATHER_X, WEATHER_Y = 5, 22  # Position weather at the bottom

# Previous values to track changes
last_time_str = None
last_date_str = None
last_weather_str = None

def update_clock():
    """Update the clock canvas only if the time changes."""
    global last_time_str
    now = datetime.now()
    time_str = now.strftime("%-I:%M %p")

    if time_str != last_time_str:  # Update only if the time changes
        last_time_str = time_str
        clock_draw.rectangle((0, 0, CLOCK_WIDTH, CLOCK_HEIGHT), fill="black")  # Clear clock canvas
        text_width, text_height = clock_draw.textbbox((0, 0), time_str, font=clock_font)[2:]
        x = (CLOCK_WIDTH - text_width) // 2
        y = (CLOCK_HEIGHT - text_height) // 2
        clock_draw.text((x, y), time_str, fill=(255, 0, 255), font=clock_font)  # Magenta
        full_canvas.paste(clock_canvas, (CLOCK_X, CLOCK_Y))  # Merge updated clock canvas


def update_date():
    """Update the date canvas only if the date changes."""
    global last_date_str
    now = datetime.now()
    date_str = now.strftime("%a, %b %-d")

    if date_str != last_date_str:  # Update only if the date changes
        last_date_str = date_str
        date_draw.rectangle((0, 0, DATE_WIDTH, DATE_HEIGHT), fill="black")  # Clear date canvas
        text_width, text_height = date_draw.textbbox((0, 0), date_str, font=date_font)[2:]
        x = (DATE_WIDTH - text_width) // 2
        y = (DATE_HEIGHT - text_height) // 2
        date_draw.text((x, y), date_str, fill=(0, 255, 255), font=date_font)  # Cyan
        full_canvas.paste(date_canvas, (DATE_X, DATE_Y))  # Merge updated date canvas
def update_weather():
    """Update the weather canvas only if the weather changes."""
    global last_weather_str
    def convert_temp(x):
        return (x * 9/5) + 32
    try:
        with open('/home/psingh/matrix-dashboard/weatherData.json', 'r') as d:
            data = json.load(d)
        data_needed = ["temperature", "temperatureApparent", "precipitationProbability", "humidity"]

        curr_details = data["timelines"]["minutely"][0]
        last_updated = curr_details["time"]
        weather_data = curr_details["values"]

        pulled_data = {}
        for i in data_needed:
            value = weather_data.get(i, None)

            if value:
                if 'temp' in i:
                    value = convert_temp(value)
                    
            pulled_data[i] = value
        
        

        curr_temp = math.ceil(pulled_data.get("temperature", None))
        feels_like = math.ceil(pulled_data.get("temperatureApparent", None))
        rain_posibility = pulled_data.get("precipitationProbability", None)
        humidity = pulled_data.get("humidity", None)

        curr_weather = f"{curr_temp}\u00B0F" # Actual: {feels_like}\nRain: {rain_posibility}, Humid: {humidity}"
    except (FileNotFoundError, KeyError, json.JSONDecodeError):
        curr_weather = "N/A"
    if curr_weather != last_weather_str:
        last_weather_str = curr_weather
        weather_draw.rectangle((0, 0, WEATHER_WIDTH, WEATHER_HEIGHT), fill="black")  # Clear date canvas
        text_width, text_height = weather_draw.textbbox((0, 0),curr_weather, font=weather_font)[2:]
        x = (WEATHER_WIDTH - text_width) // 2
        y = (WEATHER_HEIGHT - text_height) // 2
        weather_draw.text((x, y), curr_weather, fill=(255, 0, 0), font=weather_font)  # Red color
        full_canvas.paste(weather_canvas, (WEATHER_X, WEATHER_Y))  # Merge updated weather canvas


while True:
    # Update only sections that have changed
    update_clock()
    update_date()
    update_weather()

    # Push the full canvas to the display
    matrix.SetImage(full_canvas)

    # Wait for the next update
    time.sleep(30)