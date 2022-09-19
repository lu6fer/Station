
#define ENABLE_GxEPD2_GFX 0
// Display
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
#ifndef RST_PIN
#define RST_PIN 16
#define DC_PIN 17
#define CS_PIN 5
#define BUSY_PIN 4
// CLK 14
// DIN 13
#endif

// Wifi
#ifndef STASSID
#define STASSID "Chez_FloEtFlo"
#define STAPSK "Vyygop8j!"
//#define STASSID "Dialler"
//#define STAPSK "123456789"
#endif
// Tides
#ifndef TIDEURL
#define TIDEURL "https://tide.lu6fer.fr/66"
#define TIDEUA "Station/2.0 (ESP8266)"
#define TIDEUSER "admin"
#define TIDEPASS "Vyygop8j!1254"
#endif
/* pi/180 */
#ifndef RPD
#define RPD 1.74532925199e-2
#endif

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>

#include <GxEPD2_BW.h>
/* #include <U8g2_for_Adafruit_GFX.h>

#include <PubSubClient.h>
#include <Adafruit_BME280.h>

#include <time.h>
#include "base64.h"

#include "moon.h"
#include "icons.h" */

// Display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=5*/ SS, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));
// GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=15*/ CS_PIN, /*DC=4*/ DC_PIN, /*RST=2*/ RST_PIN, /*BUSY=5*/ BUSY_PIN));
//  GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT> display(GxEPD2_420_M01(/*CS=15*/ CS_PIN, /*DC=4*/ DC_PIN, /*RST=2*/ RST_PIN, /*BUSY=5*/ BUSY_PIN));
//  U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// BME280
/* Adafruit_BME280 bme; // use I2C interface

// Wifi
const char ssid[] PROGMEM = STASSID;
const char password[] PROGMEM = STAPSK;

// HTTP
const char tideUrl[] PROGMEM = TIDEURL;
const char tideUserAgent[] PROGMEM = TIDEUA;
const char tideUser[] PROGMEM = TIDEUSER;
const char tidePassword[] PROGMEM = TIDEPASS;
const String tide_auth = base64::encode(String(tideUser) + ":" + String(tidePassword));

// NTP
time_t now;   // this is the epoch
tm localTime; // the structure tm holds time information in a more convient way

// Moon
typedef struct MoonData
{
  uint8_t phase;
  double illumination;
} MoonData;

// Wifi
bool wifiConnected = false;
int wifiAttempt = 0;
// Display
bool timeDisplay = false;
bool pressureDisplay = false;
bool tideDisplay = false;
bool inDisplay = false;
bool outDisplay = false;
bool moonDisplay = false;

 */
const char ssid[] PROGMEM = STASSID;
const char password[] PROGMEM = STAPSK;
void setup()
{
  Serial.begin(115200);
  Serial.println("Setup");
  display.init(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());
}

void loop()
{
}