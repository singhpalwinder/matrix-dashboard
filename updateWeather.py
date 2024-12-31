#! /home/admin/venv/bin/python3
from dotenv import load_dotenv
import requests
import os
import json

def get_weather_data():
    # API Key from environment variable
    api_key = os.environ.get('tomorrowio_api_key')  # Replace with your key if not using env variables
    
    # Location (as a string, e.g., city name or coordinates)
    location = "29.74555,-95.45471"  # Replace with your desired location
    
    # Construct the URL
    url = f"https://api.tomorrow.io/v4/weather/forecast?location={location}&apikey={api_key}"
    
    # Headers for the request
    headers = {"accept": "application/json"}
    
    try:
        # Make the GET request
        response = requests.get(url, headers=headers)
        response.raise_for_status()  # Raise exception for HTTP errors (4xx/5xx)
        
        # Parse the JSON response
        data = response.json()
        
        # Save the data to a file
        with open("/home/admin/matrix-dashboard/weatherData.json", "w") as f:
            json.dump(data, f, indent=4)
        
        print("Weather data fetched successfully!")
        
    except requests.exceptions.RequestException as e:
        print(f"Error fetching weather data: {e}")

# Call the function
if __name__ =="__main__":
    load_dotenv(dotenv_path="/home/admin/.env")
    get_weather_data()