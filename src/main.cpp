#include <Arduino.h>
#include <WiFi.h>
#include <ezTime.h>
#include <Adafruit_Protomatter.h>
#include <Adafruit_SPIFlash.h>
#include <SdFat.h>
#include <Adafruit_ImageReader.h>
#include <Fonts/FreeSans9pt7b.h> 
#include <math.h>

#define WIDTH   64
#define HEIGHT  32
// degrees to radians
#define DEG2RAD 0.0174533

// MatrixPortal S3 pin config
uint8_t rgbPins[]  = {42, 40, 41, 38, 37, 39};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;



#if HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif HEIGHT == 64
#define NUM_ADDR_PINS 5
#endif

Adafruit_Protomatter matrix(
  WIDTH, 4, 1, rgbPins, NUM_ADDR_PINS, addrPins,
  clockPin, latchPin, oePin, true
);

Adafruit_FlashTransport_ESP32 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatVolume fatfs;

Timezone chicago;
WiFiServer server(80);
WiFiServer imageServer(9090);

const char* ssid     = "";
const char* password = "";
unsigned long lastUpdate = 0;
bool needsRedraw = false;
bool uploadInProgress = false;
bool turnOff = false;
uint16_t currentTextColor = 0;
String lastDrawnTime = "";
uint8_t cachedImage[32 * 32 * 2];  // 3KB buffer
bool imageLoaded = false;
bool processingRequest = false;
bool hasIconImage() {
  FatFile f = fatfs.open("/icon.bmp");
  bool exists = f && f.fileSize()> 0;
  f.close();
  
  return exists;
}
void cleanCanvas(){
  matrix.fillScreen(0);
}
int getQueryParam(String req, String key) {
  int idx = req.indexOf(key + "=");
  if (idx == -1) return -1;
  int endIdx = req.indexOf("&", idx);
  if (endIdx == -1) endIdx = req.length();
  String value = req.substring(idx + key.length() + 1, endIdx);
  return value.toInt();
}
void handleUpload(WiFiClient client) {
  uploadInProgress = true;
  // Discard HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  fatfs.remove("/icon.bmp");
  FatFile f = fatfs.open("/icon.bmp", FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open file for writing.");
    uploadInProgress = false;
    return;
  }

  Serial.println("Receiving image...");

  unsigned long start = millis();
  while (client.connected() && millis() - start < 5000) {
    while (client.available()) {
      f.write(client.read());
      start = millis();  // reset timeout
    }
  }

  f.close();
  Serial.println("Upload complete.");

  // Load into RAM
  FatFile readFile = fatfs.open("/icon.bmp");
  if (readFile && readFile.fileSize() == sizeof(cachedImage)) {
    readFile.read(cachedImage, sizeof(cachedImage));
    Serial.printf("icon size: %d bytes\n", readFile.fileSize());
    imageLoaded = true;
    
  }
  readFile.close();

  uploadInProgress = false;
  needsRedraw = true;
}
void saveCurrentColorToFlash(uint16_t color) {
  fatfs.remove("/color.dat");
  
  FatFile f = fatfs.open("/color.dat", FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open color.dat for writing");
    return;
  }
  f.write((uint8_t *)&color, sizeof(color));
  f.close();
  Serial.println("Color saved to flash");
}
void loadColorFromFlash() {
  FatFile f = fatfs.open("/color.dat", FILE_READ);
  if (!f) {
    Serial.println("No saved color found. Using default.");
    return;
  }
  if (f.fileSize() == sizeof(currentTextColor)) {
    f.read((uint8_t *)&currentTextColor, sizeof(currentTextColor));
    Serial.println("Loaded saved color from flash.");
  } else {
    Serial.println("color.dat file size mismatch. Using default color.");
  }
  f.close();
}
void handleReset() {
  fatfs.remove("/icon.bmp");
  imageLoaded = false;         // ✅ tells loop() to show time
  needsRedraw = true;          // ✅ triggers a refresh next frame
  Serial.println("Image reset.");
}
void handleSleepMode(bool off){
  // if command is off but matrix isnt off then power it down
  if (off && !turnOff) {
    matrix.fillScreen(0);
    matrix.show();
    turnOff = true;
    Serial.println("Sleep mode ON: Clock turned off.");
    // if command is turn on and the matrix is off then pwoer it back on
  } else if (!off && turnOff) {
    turnOff = false;
    needsRedraw = true;
    Serial.println("Sleep mode OFF: Clock resumed.");
  }

}
void serverTask(void *parameter) {
  while (true) {
    // HTTP server on port 80
    WiFiClient httpClient = server.available();
    if (httpClient) {
      Serial.println("HTTP client connected");
      processingRequest = true;

      String req = httpClient.readStringUntil('\r');
      if (req.indexOf("POST /reset") >= 0) {
        handleReset();
        needsRedraw = true;
      } else if (req.indexOf("GET /sleep/on") >= 0) {
        handleSleepMode(true);
      } else if (req.indexOf("GET /sleep/off") >= 0) {
        handleSleepMode(false);
      } else if (req.indexOf("GET /color?") >= 0) {
        int r = getQueryParam(req, "r");
        int g = getQueryParam(req, "g");
        int b = getQueryParam(req, "b");
        if (r >= 0 && g >= 0 && b >= 0) {
          currentTextColor = matrix.color565(r, g, b);
          saveCurrentColorToFlash(currentTextColor);
          needsRedraw = true;
        }
      }
      httpClient.println("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
      httpClient.stop();
      processingRequest = false;
    }

    // TCP image server on port 9090
    WiFiClient imageClient = imageServer.available();
    if (imageClient) {
      Serial.println("TCP client connected for image upload");

      int index = 0;
      while (imageClient.connected()) {
        while (imageClient.available()) {
          if (index < sizeof(cachedImage)) {
            cachedImage[index++] = imageClient.read();
          } else {
            imageClient.read();
          }
        }
      }

      Serial.printf("Received %d bytes\n", index);
      imageLoaded = true;
      needsRedraw = true;

      imageClient.stop();
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); //This line prints the IP
  waitForSync();
  chicago.setLocation("America/Chicago");
  currentTextColor = matrix.color565(150,150,150);
  if (!flash.begin()) Serial.println("Failed to init QSPI flash!");
  if (!fatfs.begin(&flash)) Serial.println("Failed to mount FAT FS!");

  loadColorFromFlash();

  ProtomatterStatus status = matrix.begin();
  xTaskCreatePinnedToCore(
    serverTask,    // Task function
    "ServerTask",  // Task name
    8192,              // Stack size
    NULL,              // Task input parameter
    1,                 // Priority
    NULL,              // Task handle
    1                  // Core 1 (WiFi core)
  );
  Serial.printf("Matrix begin() status: %d\n", status);
  matrix.setTextWrap(false);
  matrix.setFont(&FreeSans9pt7b);
  //rgb 
  matrix.setTextColor(currentTextColor); // blue

  // start both servers 
  server.begin();
  imageServer.begin();
  Serial.println("imageServer started.");
}
void getTextCenterPos(const String &text, int &x, int &y) {
  int16_t  x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  x = (WIDTH - w) / 2-2;
  y = (HEIGHT + h) / 2 - 2;  // -2 helps visually center better
}
void loop() {
  events(); // ezTime maintenance
  if (turnOff) return; 

  if (WiFi.status() != WL_CONNECTED || uploadInProgress || processingRequest || imageServer.hasClient()) {
    // Skip frame update
    return;
  }

  if ((millis() - lastUpdate > 1000 || needsRedraw)) {
    lastUpdate = millis();
    needsRedraw = false;
    
    cleanCanvas();

    String hourStr = chicago.dateTime("g");
    String minStr  = chicago.dateTime("i");
    String ampmStr = chicago.dateTime("A");
    String timeStr = hourStr + ":" + minStr + ampmStr;

    if (timeStr != lastDrawnTime || imageLoaded  || !imageLoaded) {
      
      
      if (!imageLoaded || uploadInProgress) {
        matrix.setTextColor(currentTextColor);
        // Horizontal centered
        int x, y;
        getTextCenterPos(timeStr, x, y);
        matrix.setCursor(x, y);
        matrix.print(timeStr);

      } else {
        // Image on left 32x32
        uint16_t *framebuffer = matrix.getBuffer();
        int index = 0;
        for (int y = 0; y < 32; y++) {
          for (int x = 0; x < 32; x++) {
            uint8_t high = cachedImage[index++];
            uint8_t low = cachedImage[index++];
            uint16_t color = (high << 8) | low;
            framebuffer[y * WIDTH + x] = color;
          }
        }


      int cx = 47;  // center x (right side of 64x32 matrix)
      int cy = 15;  // center y (middle of 32px height)
      int radius = 15;
      matrix.drawCircle(cx, cy, radius, matrix.color565(250, 250,250));  // Blue border
      matrix.fillCircle(cx, cy, 1, matrix.color565(250, 250,250)); // center circle
      int hour = chicago.hourFormat12();
      int minute = chicago.minute();
      //int second = chicago.second();

      // Normalize to angles
      float hour_angle = ((hour % 12) + (minute / 60.0)) * 30.0;     // 360/12 = 30 deg per hour
      float minute_angle = minute * 6.0;                              // 360/60
      //float second_angle = second * 6.0;

      // Convert to radians
      float hr = DEG2RAD * (hour_angle - 90);
      float mr = DEG2RAD * (minute_angle - 90);
     // float sr = DEG2RAD * (second_angle - 90);

      // Compute endpoints
      int hx = cx + cos(hr) * (radius - 7);
      int hy = cy + sin(hr) * (radius - 7);

      int mx = cx + cos(mr) * (radius - 5);
      int my = cy + sin(mr) * (radius - 5);

      //int sx = cx + cos(sr) * (radius - 1);
      //int sy = cy + sin(sr) * (radius - 1);
      for (int i = 0; i < 12; i++) {
        float angle = DEG2RAD * (i * 30 - 90);
        int x1 = cx + cos(angle) * (radius - 1);
        int y1 = cy + sin(angle) * (radius - 1);
        int x2 = cx + cos(angle) * (radius - 3);
        int y2 = cy + sin(angle) * (radius - 3);
        matrix.drawLine(x1, y1, x2, y2, matrix.color565(250, 90,10));
      }

      // Draw hands
      matrix.drawLine(cx, cy, hx, hy, matrix.color565(250, 90,10));  // Hour hand - white
      matrix.drawLine(cx, cy, mx, my, matrix.color565(250, 90,10));      // Minute hand - green
      //matrix.drawLine(cx, cy, sx, sy, matrix.color565(255, 0, 0));      // Second hand - red
      }

      matrix.show();
      lastDrawnTime = timeStr;
    }
  }
}