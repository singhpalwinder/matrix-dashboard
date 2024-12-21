from rgbmatrix import RGBMatrix, RGBMatrixOptions
from PIL import Image, ImageDraw, ImageFont
from datetime import datetime
import time, config
# Configuration options
options = RGBMatrixOptions()
options.rows = 32
options.cols = 64
options.chain_length = 1
options.parallel = 1
options.brightness = config.brightness
options.hardware_mapping = "adafruit-hat"

matrix = RGBMatrix(options=options)

# Load the font
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 12)

# Swap green and blue channels
def swap_green_blue(img):
    r, g, b = img.split()  # Split into individual channels
    return Image.merge("RGB", (r, b, g))  # Swap green and blue


# Main loop to update the time
last_time_str = None  # Keep track of the last displayed time

while True:
    # Get the current time in the desired format
    now = datetime.now()
    time_str = now.strftime("%-I:%M%p")  # %-I removes leading zero for the hour

    # Only update if the time has changed
    if time_str != last_time_str:
        last_time_str = time_str

        # Create a blank image
        image = Image.new("RGB", (64, 32), "black")
        draw = ImageDraw.Draw(image)

        # Get text size and position it in the center
        text_width, text_height = draw.textbbox((0, 0), time_str, font=font)[2:]
        x = (64 - text_width) // 2
        y = (32 - text_height) // 2

        # Draw the time on the image
        draw.text((x, y), time_str, fill=(255, 255, 0), font=font)

        # Swap green and blue channels
        swapped_image = swap_green_blue(image)

        # Display the image
        matrix.SetImage(swapped_image)

    # Wait for a second before checking the time again
    time.sleep(1)
