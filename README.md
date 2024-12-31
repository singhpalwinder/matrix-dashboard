# matrix-display

# corntab entry for weather update every 10 minutes 
*/10 * * * * echo "----$(date)----" > /home/admin/matrix-dashboard/logs/updateWeather.log && /home/admin/venv/bin/python3 /home/admin/matrix-dashboard/updateWeather.py >> /home/admin/matrix-dashboard/logs/updateWeather.log 2>&1