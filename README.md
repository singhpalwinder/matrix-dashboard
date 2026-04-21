# Matrix Portal S3 Artwork + Clock Display

This project runs on the [Adafruit Matrix Portal S3](https://www.adafruit.com/product/5778), a dual-core ESP32-S3 board designed for driving RGB matrix displays. It combines image display and a real-time analog clock rendered directly on the LED matrix.

Artwork is sent to this device from a separate project:
🔗 [shairport-metadata](https://github.com/singhpalwinder/shairport-metadata)

---

## ✨ Features

- 🔁 Displays static images or animated GIFs sent via TCP
- 🕒 Shows an analog clock with smooth hands and tick marks for all 12 hours
- 🧱 Digital clock renders in a retro Mario-style pastel palette with a tetris-falling animation on every minute change
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
```

---

## 🧱 Tetris-Style Digital Clock

When the device is in digital-clock mode, every minute tick plays a falling-block animation instead of instantly swapping the digits.

- **Whole-clock rewrite**: all currently-lit pixels break into tetromino-shaped pieces (I, O, L, J, T, S and partial strips) and fall off the bottom of the matrix, while a fresh set of pieces falls in from above to form the new time. Old and new pieces cross mid-air.
- **Retro palette**: every piece draws a color from a shuffled 12-color pastel/NES-Mario deck (peach, mint, sky, lavender, rose, lemon, turquoise, salmon, periwinkle, pistachio, mario red, koopa yellow). The deck is reshuffled each cycle so neighbors rarely share a color and the whole palette is visited before any color repeats.
- **Staggered motion**: each piece picks its own start time (0–1.8 s out, 0.4–2.6 s in) and its own fall speed (75–129 ms per cell), so the animation doesn't read as one block sliding down — pieces drift apart as they fall.
- **Colors persist**: when the animation finishes, the landed retro-colored pixels stay on screen until the next minute tick. The digital view skips its per-second idle repaint so the colors aren't clobbered by a flat-color redraw.
- **Same look on boot**: the very first render (both the pre-NTP `12:00AM` placeholder and the first post-sync time) is already painted with the same deck-colored pieces, so there's no white-to-color transition when the device starts.
- **View modes unaffected**: image / GIF modes still use the existing dissolve transition on mode switches and still render the analog clock overlay on every second tick.

Relevant source:

- `include/tetris_transition.h` / `src/tetris_transition.cpp` — palette, piece chunking, falling animation, static colored render
- `src/main.cpp` — triggers `startForDigital(...)` on minute change and routes digital renders through `renderColored(...)`