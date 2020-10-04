// 4x4 bottle light red/orange matrix led glow
//
// Note: this is a quickly hacked togehter piece of code. Might change in future, as it's not ready yet.
// See blog post: https://www.kaper.com/electronics/glowing-bottle/
//
// Known Issue:
// - write of config changes crashes the ESP, need to use one big file, or switch to new file-system.
//
// Change plans:
// - Add option to change all colors in use, and test them one by one.
// - Add option to turn on/off the light.
// - Perhaps add voice control? (e.g. "alexa - turn off bottle lamp").
// - Perhaps add timed on/off schedule? (on during day, off during night).
// 
// 4 October 2020, Thijs Kaper
// 

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Time.h>
#include <Syslog.h>
#include <FS.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif

// put your WiFi SSID and Password here
const char* ssid = "XXXX";
const char* password = "XXXX";

ESP8266WebServer server ( 80 );

int brightness = 40;


#define PIN D5

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)


// Example for NeoPixel Shield.  In this application we'd like to use it
// as a 5x8 tall matrix, with the USB port positioned at the top of the
// Arduino.  When held that way, the first pixel is at the top right, and
// lines are arranged in columns, progressive order.  The shield uses
// 800 KHz (v2) pixels that expect GRB color data.
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(4, 4, PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB            + NEO_KHZ800);


// set to number of elements in colors array
const int colors_size = 7;

uint16_t colors[] = {
  matrix.Color(255, 0, 0), 
  matrix.Color(255, 0, 0), 
//  matrix.Color(255, 205, 0), 
  matrix.Color(255, 180, 0),
  matrix.Color(200, 0, 0),
  matrix.Color(150, 80, 0),
  matrix.Color(150, 0, 0), 
  matrix.Color(0, 0, 0)
};

// storage for all pixel colors (new is where we want to go, old is the current color)
uint16_t pixels_new[16];
uint16_t pixels_old[16];

// loop_delay is the sleep time between events
int loop_delay_low = 8;
int loop_delay_high = 25;

// pixelchange_divisor determines how often to change some random pixels
int pixelchange_divisor_low = 30;
int pixelchange_divisor_high = 50;

// pixelchange_count determines how many pixels we change at a time
int pixelchange_count_low = 3;
int pixelchange_count_high = 10;

// If true (1), change ALL pixles each time, if false (0), use pixelchange_count range of pixels to change
int changeAll = 0;

// brightness range
int brightness_low = 70;
int brightness_high = 150;

// brightness_divisor determines how often to change brighness
int brightness_divisor_low = 3;
int brightness_divisor_high = 6;


int readConfigValue(char *name, int defaultValue) {
  Serial.println("readConfigValue");
  Serial.println(name);
  
  File file = SPIFFS.open(name, "r");
  if (!file) return defaultValue;
  String line = file.readStringUntil('\n');
  file.close();
  Serial.println(line);
  return line.toInt();
}

// Ok, I tried writing a separate file per value (this code below), but...
// that does not seem to be a good idea. The ESP crashes when writing all values.
// My guess is that it needs to give some time back to handle wifi and other important
// stuff in between writing. I do need to look at that to fix this.
// Or perhaps write one big file with all settings, or switch to the new filesystem (spiffs is deprecated?).
// Sometime later... (if ever?)
void writeConfigValue(char *name, int value) {
  Serial.println("writeConfigValue");
  Serial.println(name);
  Serial.println(value);

  File file = SPIFFS.open(name, "w");
  if (!file) return;
  file.println(value);
  file.close();
}

void readConfig() {
  bool result = SPIFFS.begin();
  Serial.println("SPIFFS opened: " + result);
  if (!result) return;

  loop_delay_low              = readConfigValue("loop_delay_low", loop_delay_low);
  loop_delay_high             = readConfigValue("loop_delay_high", loop_delay_high);
  pixelchange_divisor_low     = readConfigValue("pixelchange_divisor_low", pixelchange_divisor_low);
  pixelchange_divisor_high    = readConfigValue("pixelchange_divisor_high", pixelchange_divisor_high);
  pixelchange_count_low       = readConfigValue("pixelchange_count_low", pixelchange_count_low);
  pixelchange_count_high      = readConfigValue("pixelchange_count_high", pixelchange_count_high);
  changeAll                   = readConfigValue("changeAll", changeAll);
  brightness_low              = readConfigValue("brightness_low", brightness_low);
  brightness_high             = readConfigValue("brightness_high", brightness_high);
  brightness_divisor_low      = readConfigValue("brightness_divisor_low", brightness_divisor_low);
  brightness_divisor_high     = readConfigValue("brightness_divisor_high", brightness_divisor_high);
}

void setup() {
  String bootReason = ESP.getResetInfo();

  matrix.begin();
  matrix.setBrightness(40);
  matrix.fillScreen(0);
  matrix.show();

  Serial.begin(115200); 
  Serial.println("\n\nBooting\n\n");
  Serial.println(bootReason);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("led4x4");
  // put your OTA password here:
  ArduinoOTA.setPassword((const char *)"xxx");

  ArduinoOTA.onStart([]() {
    Serial.println("Firmware update start");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char buffer[20];
    sprintf(buffer, "%3u %%", (100 * progress / total));
    Serial.println(buffer);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA end");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("\nReady");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on ( "/", handleRoot );
  server.onNotFound ( handleNotFound );
  server.begin();

  readConfig();

  // setup initial random colors
  for (int i=0;i<16;i++) {
    pixels_new[i] = colors[random(0, colors_size)];
    pixels_old[i] = pixels_new[i];
  }

  // turn off the blue on-board led... on pin D4
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH);
}

int getParameter(int currentValue, char *name) {
    if (server.arg(name).length()>0) {
      int old = readConfigValue(name, -1);
      yield();
      if (old != server.arg(name).toInt()) {
        writeConfigValue(name, server.arg(name).toInt());
        // tried adding yield to prevent crash on write, does not help. Write needs fixing.
        yield();
      }
      return server.arg(name).toInt();
    }
    return currentValue;
}

// root web page
void handleRoot() {
    loop_delay_low=getParameter(loop_delay_low, "loop_delay_low");
    loop_delay_high=getParameter(loop_delay_high, "loop_delay_high");
    pixelchange_divisor_low=getParameter(pixelchange_divisor_low, "pixelchange_divisor_low");
    pixelchange_divisor_high=getParameter(pixelchange_divisor_high, "pixelchange_divisor_high");
    pixelchange_count_low=getParameter(pixelchange_count_low, "pixelchange_count_low");
    pixelchange_count_high=getParameter(pixelchange_count_high, "pixelchange_count_high");
    changeAll=getParameter(changeAll, "changeAll");
    brightness_low=getParameter(brightness_low, "brightness_low");
    brightness_high=getParameter(brightness_high, "brightness_high");
    brightness_divisor_low=getParameter(brightness_divisor_low, "brightness_divisor_low");
    brightness_divisor_high=getParameter(brightness_divisor_high, "brightness_divisor_high");

    if (server.arg("reboot").length()>0) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
        ESP.restart();
    }

    if (server.arg("format").length()>0) {
        server.sendHeader("Location", String("/"), true);
        server.send ( 302, "text/plain", "");
        SPIFFS.format();
        ESP.restart();
    }

    // This is a bit of test code, not ready
    // the plan is to allow the user to configure ALL colors in the color array
    // This test code just shows and alters the first entry in the array.
    String rgbArg = server.arg("color");
    if (rgbArg.length()) {
      uint32_t rgb = (uint32_t) strtoull( &rgbArg[1], NULL, 16);
      uint8_t r = rgb >> 16;
      uint8_t g = rgb >> 8 & 0xFF;
      uint8_t b = rgb & 0xFF;
      colors[0] = matrix.Color(r,g,b);
    }
    
    // This is a bit of test code, not ready (see above)
    uint8_t n_red = getRed(colors[0]);
    uint8_t n_green = getGreen(colors[0]);
    uint8_t n_blue = getBlue(colors[0]);
    uint32_t color = n_red << 16 | n_green << 8 | n_blue;
    

  // render web page to show configuration
  char temp[2400];
  snprintf ( temp, 2400,
"<!doctype html><html>\n\
  <head>\n\
    <meta http-equiv='content-type' content='text/html; charset=ISO-8859-1' />\n\
    <title>led-4x4</title>\n\
    <style>\n\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\n\
      form { border: 1px solid black; width: 350px; }\n\
    </style>\n\
    <script src='https://www.kaper.com/jscolor/jscolor.js'></script>\n\
    <meta name='viewport' content='width=device-width, initial-scale=1.0, xmaximum-scale=1.0'>\n\
</head>\n\
  <body>\n\
    <h3>led-4x4</h3>\n\
        <form action='/' method='post'>\n\
           loop_delay_low: <input type='input' name='loop_delay_low' size='3' value='%d'/><br/>\n\
           loop_delay_high: <input type='input' name='loop_delay_high' size='3' value='%d'/><br/>\n\
           pixelchange_divisor_low: <input type='input' name='pixelchange_divisor_low' size='3' value='%d'/><br/>\n\
           pixelchange_divisor_high: <input type='input' name='pixelchange_divisor_high' size='3' value='%d'/><br/>\n\
           pixelchange_count_low: <input type='input' name='pixelchange_count_low' size='3' value='%d'/><br/>\n\
           pixelchange_count_high: <input type='input' name='pixelchange_count_high' size='3' value='%d'/><br/>\n\
           changeAll: <input type='input' name='changeAll' size='1' value='%d'/><br/>\n\
           brightness_low: <input type='input' name='brightness_low' size='3' value='%d'/><br/>\n\
           brightness_high: <input type='input' name='brightness_high' size='3' value='%d'/><br/>\n\
           brightness_divisor_low: <input type='input' name='brightness_divisor_low' size='3' value='%d'/><br/>\n\
           brightness_divisor_high: <input type='input' name='brightness_divisor_high' size='3' value='%d'/><br/>\n\
           <input type='submit' value='update'/> | <a href='/'>(cancel/refresh/reload)</a> | <a href='/?reboot=true'>(reboot)</a>\n\
        </form>\n\
        <hr>\n\
        <form action='/' method='post'>\n\
           Color: <input type='input' name='color' class='jscolor' data-jscolor='' value='#%06X'><br/>\n\
           <input type='submit' value='update'/>\n\
        </form>\n\
  </body>\n\
</html>",
    loop_delay_low,loop_delay_high,pixelchange_divisor_low,pixelchange_divisor_high,pixelchange_count_low,pixelchange_count_high,
    changeAll,brightness_low,brightness_high,brightness_divisor_low,brightness_divisor_high,color
  );
    server.send ( 200, "text/html", temp);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}


int pixelchange_divisor_counter=pixelchange_divisor_low;
int newBrightness=brightness_low;
int brightness_divisor_counter=brightness_divisor_low;

void loop() {
   ArduinoOTA.handle();
   server.handleClient();

   // render all 4x4 pixels on the display
   for (int x=0; x<4; x++) {
     for (int y=0; y<4; y++) {
       matrix.drawPixel(x, y, pixels_old[x+(4*y)]);
     }
   }
   matrix.setBrightness(brightness);
   matrix.show();

   // when it's time to change one or more colors, do it...
   if (pixelchange_divisor_counter-- < 0) {
       pixelchange_divisor_counter=random(pixelchange_divisor_low, pixelchange_divisor_high);
       if (changeAll) {
          // change ALL pixels:
          for (int i=0;i<16;i++) {
            pixels_new[i] = colors[random(0, colors_size)];
          }
       } else {
         // change some pixels:
         for (int n=0;n<random(pixelchange_count_low, pixelchange_count_high);n++) {
           pixels_new[random(0, 16)] = colors[random(0, colors_size)];
         }
       }
   }

   // correct old clor to look more like new color on every iteration
    for (int i=0;i<16;i++) {
      uint16_t n = pixels_new[i];
      uint16_t o = pixels_old[i];

      uint8_t n_red = getRed(n);
      uint8_t n_green = getGreen(n);
      uint8_t n_blue = getBlue(n);
      
      uint8_t o_red = getRed(o);
      uint8_t o_green = getGreen(o);
      uint8_t o_blue = getBlue(o);

      // The adafruit library removes lowest bits from the colors, so the correction steps are taken to ignore the lower bits.
      // e.g. step sizes: red = 8, green = 4, and blue = 8
      
      if (o_red>n_red) o_red=o_red-8;
      if (o_green>n_green) o_green=o_green-4;
      if (o_blue>n_blue) o_blue=o_blue-8;
      
      if (o_red<n_red) o_red=o_red+8;
      if (o_green<n_green) o_green=o_green+4;
      if (o_blue<n_blue) o_blue=o_blue+8;

      pixels_old[i] = matrix.Color(o_red, o_green, o_blue);      
    }

    // also change brightness of all leds randomly
    if(brightness_divisor_counter-- <= 0) {
      brightness_divisor_counter = random(brightness_divisor_low, brightness_divisor_high);
      if (brightness==newBrightness) newBrightness = random(brightness_low, brightness_high);
      if (brightness>newBrightness) brightness = brightness - 1;
      if (brightness<newBrightness) brightness = brightness + 1;
    }

  // random sleep
  delay(random(loop_delay_low, loop_delay_high));
}

// Code in adafruit does this for each pixel color - so I have to reverse that...
//
//// Downgrade 24-bit color to 16-bit (add reverse gamma lookup here?)
//uint16_t Adafruit_NeoMatrixPlus::Color(uint8_t r, uint8_t g, uint8_t b) {
//  return ((uint16_t)(r & 0xF8) << 8) |
//         ((uint16_t)(g & 0xFC) << 3) |
//                    (b         >> 3);
//}

uint8_t getRed(uint16_t color) {
  return getColorPart(color, 8, 0xF8);
}

uint8_t getGreen(uint16_t color) {
  return getColorPart(color, 3, 0xFC);
}

uint8_t getBlue(uint16_t color) {
  return (uint8_t) ( ( (uint16_t) (color << 3) ) & ((uint16_t) 0xff));
}

uint8_t getColorPart(uint16_t color, int shiftRight, uint8_t mask) {
  return (uint8_t) ( ( (uint16_t) (color >> shiftRight) ) & ((uint16_t) mask));
}
