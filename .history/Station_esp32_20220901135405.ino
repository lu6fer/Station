
#define ENABLE_GxEPD2_GFX 1
// Display
#ifndef RST_PIN
#define RST_PIN 5
#define DC_PIN 0
#define CS_PIN 15
#define BUSY_PIN 4
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
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <PubSubClient.h>
#include <Adafruit_BME280.h>

#include <time.h>
#include "base64.h"

#include "moon.h"
#include "icons.h"

void setup()
{
  // put your setup code here, to run once:
}

void loop()
{
  // put your main code here, to run repeatedly:
}
