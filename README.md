# Matrix Portal S3 Artwork + Clock Display

This project runs on the [Adafruit Matrix Portal S3](https://www.adafruit.com/product/5778), a dual-core ESP32-S3 board designed for driving RGB matrix displays. It combines image display and a real-time analog clock rendered directly on the LED matrix.

Artwork is sent to this device from a separate project:
🔗 [shairport-metadata](https://github.com/singhpalwinder/shairport-metadata)

---

## ✨ Features

- 🔁 Displays static images or animated GIFs sent via TCP
- 🕒 Shows an analog clock with smooth hands and tick marks for all 12 hours
- 📶 Connects to Wi-Fi and syncs time via NTP using `ezTime`
- 🎨 User-defined text color saved to onboard QSPI flash
- 🔌 Upload images via HTTP (`/icon.bmp`) or TCP (GIF frames)
- 🌙 Sleep mode support (on/off toggle via HTTP/Homeassistant)
- 🔄 Dual-core support: one core handles drawing, the other handles network/server logic

---

## 🧠 How It Works

- **Matrix Rendering**: Uses the [Adafruit_Protomatter](https://github.com/adafruit/Adafruit_Protomatter) library for high-performance drawing on 64x32 RGB matrices.
- **Timekeeping**: `ezTime` manages timezones and NTP syncing.
- **Trigonometry Logic**: The analog clock uses SOH-CAH-TOA methodology to calculate hand positions using `sin` and `cos`.

Each frame is processed using the `matrix.getBuffer()` method, where pixels are updated manually for maximum control. An analog clock is drawn using circular geometry and rendered on the right side of the screen.

---

## 📷 Image & GIF Uploading

### 📤 HTTP Upload (Single Static Image)

Upload a 32x32 `icon.gif` via POST request to port `80` or image artwork to port `9090` via tcp socket:

```bash
curl -X POST --data-binary "@icon.bmp" http://<DEVICE_IP>/upload