#! /home/psingh/matrix-dashboard/venv/bin/python3
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
from PIL import Image, ImageDraw, ImageFont
from datetime import datetime
import time, config, json, math


# Full matrix dimensions
MATRIX_WIDTH = 64
MATRIX_HEIGHT = 32

# Section dimensions (example: clock area 38x8, date area 24x8)
DATE_WIDTH, DATE_HEIGHT = 60, 12
CLOCK_WIDTH, CLOCK_HEIGHT = 42, 10
WEATHER_WIDTH, WEATHER_HEIGHT = 42, 15

# Positioning of each section on the full matrix
DATE_X, DATE_Y = 5, 0      # Position date at the top
CLOCK_X, CLOCK_Y = 18, 13  # Center the clock
WEATHER_X, WEATHER_Y = 18, 22  # Position weather at the bottom

# Configuration options for the RGB Matrix
options = RGBMatrixOptions()
options.rows = 32
options.cols = 64
options.chain_length = 1
options.parallel = 1
options.brightness = 50
options.hardware_mapping = "adafruit-hat"
options.gpio_slowdown = 2
matrix = RGBMatrix(options=options)

# Load BDF fonts
date_font = graphics.Font()
date_font.LoadFont("/home/psingh/rpi-rgb-led-matrix/fonts/5x7.bdf")  # Adjust font path
clock_font = graphics.Font()
clock_font.LoadFont("/home/psingh/rpi-rgb-led-matrix/fonts/5x7.bdf")  # Adjust font path
weather_font = graphics.Font()
weather_font.LoadFont("/home/psingh/rpi-rgb-led-matrix/fonts/4x6.bdf")  # Adjust font path

# Define colors for the fonts
clock_color = graphics.Color(255, 0, 255)  # Magenta
date_color = graphics.Color(0, 255, 255)  # Cyan
weather_color = graphics.Color(255, 0, 0)  # Red



# Previous values to track changes
last_time_str = None
last_date_str = None
last_weather_str = None

def update_date():
    """Update the date on the matrix."""
    global last_date_str
    now = datetime.now() 
    date_str = now.strftime("%a, %b %-d")

    if date_str != last_date_str:  # Update only if the date changes
        last_date_str = date_str
        for x in range(DATE_X, DATE_X + DATE_WIDTH):
            for y in range(DATE_Y, DATE_Y + DATE_HEIGHT):
                matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black
        x = DATE_X  # Starting x-coordinate
        y = DATE_Y + 10  # Starting y-coordinate (adjust to align text)
        graphics.DrawText(matrix, date_font, x, y, date_color, date_str)
def update_clock():
    """Update the clock on the matrix."""
    global last_time_str
    now = datetime.now()
    time_str = now.strftime("%-I:%M%p")  # Add a space between time and AM/PM

    if time_str != last_time_str:  # Update only if the time changes
        # Clear the entire clock area
        for x in range(CLOCK_X, CLOCK_X + CLOCK_WIDTH):
            for y in range(CLOCK_Y, CLOCK_Y + CLOCK_HEIGHT):
                matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black

        # Update the last time string
        last_time_str = time_str

        # Draw the updated time
        x = CLOCK_X  # Starting x-coordinate
        y = CLOCK_Y + 7  # Adjust to align with font baseline
        graphics.DrawText(matrix, clock_font, x, y, clock_color, time_str)

def update_weather():
    """Update the weather display only if the weather changes."""
    global last_weather_str

    def convert_temp(x):
        return (x * 9 / 5) + 32

    try:
        # Load weather data from JSON
        with open('/home/psingh/matrix-dashboard/weatherData.json', 'r') as d:
            data = json.load(d)

        data_needed = ["temperature", "temperatureApparent", "precipitationProbability", "humidity"]
        curr_details = data["timelines"]["minutely"][0]
        weather_data = curr_details["values"]

        # Extract and process needed data
        pulled_data = {}
        for i in data_needed:
            value = weather_data.get(i, None)
            if value:
                if 'temp' in i:
                    value = convert_temp(value)
            pulled_data[i] = value

        curr_temp = math.ceil(pulled_data.get("temperature", 'N/A'))
        humidity = pulled_data.get("humidity", 'N/A')
        curr_weather = f"{curr_temp}\u00B0F {humidity}%"  # Additional details can be added if needed
    except (FileNotFoundError, KeyError, json.JSONDecodeError):
        curr_weather = "N/A"

    if curr_weather != last_weather_str:
        # Clear the weather area
        for x in range(WEATHER_X, WEATHER_X + WEATHER_WIDTH):
            for y in range(WEATHER_Y, WEATHER_Y + WEATHER_HEIGHT):
                matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black

        # Update the last weather string
        last_weather_str = curr_weather

        # Draw the updated weather
        x = WEATHER_X  # Starting x-coordinate
        y = WEATHER_Y + 7  # Adjust to align with font baseline
        graphics.DrawText(matrix, weather_font, x, y, weather_color, curr_weather)


while True:
    # Update only sections that have changed
    update_clock()
    update_date()
    update_weather()


    # Wait for the next update
    time.sleep(30)