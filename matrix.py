#! /home/admin/venv/bin/python3
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
from datetime import datetime
import time, config, json, math

class Clock:
    def __init__(self):
        # Previous values to track changes used as global values
        self.last_time_str = None
        self.last_date_str = None
        self.last_weather_str = None

    def update_date(self, matrix, font,date_color, date_x, date_y, date_width, date_height):
        """Update the date on the matrix."""
        global last_date_str
        now = datetime.now() 
        date_str = now.strftime("%a, %b %-d")

        if date_str != self.last_date_str:  # Update only if the date changes
            self.last_date_str = date_str
            for x in range(date_x, date_x + date_width):
                for y in range(date_y, date_y + date_height):
                    matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black
            x = date_x  # Starting x-coordinate
            y = date_y + 10  # Starting y-coordinate (adjust to align text)
            graphics.DrawText(matrix, font, x, y, date_color, date_str)
    def update_clock(self, matrix, font, clock_color, clock_x, clock_y, clock_width, clock_height):
        """Update the clock on the matrix."""
        global last_time_str
        now = datetime.now()
        time_str = now.strftime("%-I:%M%p")  # Add a space between time and AM/PM

        if time_str != self.last_time_str:  # Update only if the time changes
            # Clear the entire clock area
            for x in range(clock_x, clock_x + clock_width):
                for y in range(clock_y, clock_y + clock_height):
                    matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black

            # Update the last time string
            self.last_time_str = time_str

            # Draw the updated time
            x = clock_x  # Starting x-coordinate
            y = clock_y + 7  # Adjust to align with font baseline
            graphics.DrawText(matrix, font, x, y, clock_color, time_str)

    def update_weather(self, matrix, font, weather_color, weather_x, weather_y, weather_width, weather_height):
        """Update the weather display only if the weather changes."""
        global last_weather_str

        def convert_temp(x):
            return (x * 9 / 5) + 32

        try:
            # Load weather data from JSON
            with open('/home/admin/matrix-dashboard/weatherData.json', 'r') as d:
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

        if curr_weather != self.last_weather_str:
            # Clear the weather area
            for x in range(weather_x, weather_x + weather_width):
                for y in range(weather_y, weather_y + weather_height):
                    matrix.SetPixel(x, y, 0, 0, 0)  # Set pixels to black

            # Update the last weather string
            self.last_weather_str = curr_weather

            # Draw the updated weather
            x = weather_x  # Starting x-coordinate
            y = weather_y + 7  # Adjust to align with font baseline
            graphics.DrawText(matrix, font, x, y, weather_color, curr_weather)

    def ShowClock(self):
        # Full matrix dimensions
        matrix_width = 64
        matrix_height = 32

        # Section dimensions (example: clock area 38x8, date area 24x8)
        date_width, date_height = 60, 15
        clock_width, clock_height = 42, 10
        weather_width, weather_height = 42, 10


        weather_x, weather_y = 17, 0  # Position weather center middle
        clock_x, clock_y = 18, 11  # Center the clock in the middle
        date_x, date_y = 5, 20     # Position date at the bottom


        # Configuration options for the RGB Matrix
        options = RGBMatrixOptions()
        options.rows = matrix_height
        options.cols = matrix_width
        options.chain_length = 1
        options.parallel = 1
        options.brightness = config.brightness
        options.hardware_mapping = "adafruit-hat"
        options.gpio_slowdown = 4
        matrix = RGBMatrix(options=options)

        # Load BDF fonts
        date_font = graphics.Font()
        date_font.LoadFont("/home/admin/rpi-rgb-led-matrix/fonts/5x7.bdf")  # Adjust font path
        clock_font = graphics.Font()
        clock_font.LoadFont("/home/admin/rpi-rgb-led-matrix/fonts/5x7.bdf")  # Adjust font path
        weather_font = graphics.Font()
        weather_font.LoadFont("/home/admin/rpi-rgb-led-matrix/fonts/4x6.bdf")  # Adjust font path

        # Define colors for the fonts (RGB)
        clock_color = graphics.Color(255, 0, 255)  # Magenta
        date_color = graphics.Color(0, 255, 255)  # Cyan
        weather_color = graphics.Color(255, 0, 0)  # Red

        # all functions take args (matrix, font, font_color, x_pos, y_pos, width, height)
        while True:
            # Update only sections that have changed
            self.update_clock(matrix, clock_font, clock_color, clock_x, clock_y,clock_width, clock_height)
            self.update_date(matrix, date_font, date_color, date_x, date_y,date_width, date_height)
            self.update_weather(matrix, weather_font, weather_color, weather_x, weather_y,weather_width, weather_height)


            # Wait for the next update
            time.sleep(30)
if __name__ == "__main__":
    c = Clock()
    c.ShowClock()