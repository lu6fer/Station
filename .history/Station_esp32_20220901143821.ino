
#define ENABLE_GxEPD2_GFX 1
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

// Display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=15*/ CS_PIN, /*DC=4*/ DC_PIN, /*RST=2*/ RST_PIN, /*BUSY=5*/ BUSY_PIN));
// GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT> display(GxEPD2_420_M01(/*CS=15*/ CS_PIN, /*DC=4*/ DC_PIN, /*RST=2*/ RST_PIN, /*BUSY=5*/ BUSY_PIN));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Wifi
WiFiMulti Wifi;

// BME280
Adafruit_BME280 bme; // use I2C interface

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

/*
 * Moon phase
 */
MoonData calculateMoonData(uint16_t year, uint8_t month, uint8_t day)
{
  MoonData moonData;

  // from Gregorian year, month, day, calculate the Julian Day number
  uint8_t c;
  uint32_t jd;
  if (month < 3)
  {
    --year;
    month += 10;
  }
  else
    month -= 2;

  c = year / 100;
  jd += 30.59 * month;
  jd += 365.25 * year;
  jd += day;
  jd += c / 4 - c;

  // adjust to Julian centuries from noon, the day specified.
  double t = (jd - 730455.5) / 36525;

  // following calculation from Astronomical Algorithms, Jean Meeus
  // D, M, MM from (47.2, 47.3, 47.3 page 338)

  // mean elongation of the moon
  double D = 297.8501921 + t * (445267.1114034 +
                                t * (-0.0018819 + t * (1.0 / 545868 - t / 113065000)));

  // suns mean anomaly
  double M = 357.5291092 + t * (35999.0502909 + t * (-0.0001536 + t / 24490000));

  // moons mean anomaly
  double MM = 134.9633964 +
              t * (477198.8675055 + t * (0.0087414 + t * (1.0 / 69699 - t / 14712000)));

  // (48.4 p346)
  double i = 180 - D - 6.289 * sin(MM * RPD) + 2.100 * sin(M * RPD) - 1.274 * sin((2 * D - MM) * RPD) - 0.658 * sin(2 * D * RPD) - 0.214 * sin(2 * MM * RPD) - 0.110 * sin(D * RPD);

  if (i < 0)
    i = -i;
  if (i >= 360)
    i -= floor(i / 360) * 360;

  // (48.1 p 345)
  // this is the proportion illuminated calculated from `i`, the phase angle
  double k = (1 + cos(i * RPD)) / 2;

  // but for the `phase` don't use the phase angle
  // instead just consider the 0-360 cycle to get equal parts per phase
  uint8_t ki = i / 22.5;
  if (++ki == 16)
    ki = 0;
  ki = (ki / 2 + 4) & 7;

  moonData.phase = ki;
  moonData.illumination = k;

  return moonData;
}

/*
 * Display
 */

void drawBackground()
{
  Serial.println(F("Draw background"));
  display.fillScreen(GxEPD_WHITE);
  display.display();
  int thick = 4;
  int radius = 10;
  for (int i = 0; i < thick; i++)
  {
    // Top box
    display.drawRoundRect(5 + i, 5 + i, 390 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);

    // Right box
    display.drawRoundRect(5 + i, 155 + i, 190 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);

    // Left box
    display.drawRoundRect(205 + i, 155 + i, 190 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);
  }
  // display.drawBitmap(15, 160, , )
  // display.fillRect(15, 165, 16, 16, GxEPD_BLACK);
  // display.fillRect(215, 165, 16, 16, GxEPD_BLACK);
  display.drawInvertedBitmap(15, 165, icon_home, 16, 16, GxEPD_BLACK);
  display.drawInvertedBitmap(215, 165, icon_out, 16, 16, GxEPD_BLACK);
  display.display(false);
}

void displayDateTime(int year, int month, int day, int hour, int min)
{
  Serial.println(F("Display date time"));
  timeDisplay = true;

  // Clear zone
  display.fillRect(10, 20, 155, 75, GxEPD_WHITE);

  // Time
  char currentHour[5];
  sprintf(currentHour, "%02d:%02d", hour, min);

  u8g2Fonts.setFont(u8g2_font_fub42_tf);
  // u8g2Fonts.setCursor(10, 80);
  u8g2Fonts.setCursor(10, 60);
  u8g2Fonts.print(currentHour);

  // Date
  char currentDate[10];
  sprintf(currentDate, "%02d/%02d/%d", day, month, year);

  u8g2Fonts.setFont(u8g2_font_fur14_tf);
  u8g2Fonts.setCursor(30, 80);
  u8g2Fonts.print(currentDate);

  display.display(true);
}

void displayTide()
{
  Serial.println(F("Display tide"));
  tideDisplay = true;
  // Clear zone
  display.fillRect(230, 20, 155, 55, GxEPD_WHITE);
  // title
  u8g2Fonts.setFont(u8g2_font_fub11_tf);
  u8g2Fonts.setCursor(240, 30);
  u8g2Fonts.print("BM");

  u8g2Fonts.setCursor(295, 30);
  u8g2Fonts.print("PM");

  u8g2Fonts.setCursor(340, 30);
  u8g2Fonts.print("Coef");

  // first line
  u8g2Fonts.setCursor(235, 50);
  u8g2Fonts.print("11h51");

  u8g2Fonts.setCursor(285, 50);
  u8g2Fonts.print("05h39");

  u8g2Fonts.setCursor(350, 50);
  u8g2Fonts.print("49");

  // second line
  u8g2Fonts.setCursor(235, 70);
  u8g2Fonts.print("11h51");

  u8g2Fonts.setCursor(285, 70);
  u8g2Fonts.print("05h39");

  u8g2Fonts.setCursor(350, 70);
  u8g2Fonts.print("49");

  // Harbor
  u8g2Fonts.setCursor(250, 90);
  u8g2Fonts.print("Perros-Guirec");

  display.display(true);
}

void displayIn(int temp, int humidity)
{
  Serial.println(F("Display In"));
  inDisplay = true;
  // Clear zone
  display.fillRect(20, 180, 150, 100, GxEPD_WHITE);
  // Temp
  char currentTemp[5];
  sprintf(currentTemp, "%d°C", temp);
  u8g2Fonts.setFont(u8g2_font_fub42_tf);
  u8g2Fonts.setCursor(30, 230);
  u8g2Fonts.print(currentTemp);
  // Humidity
  char currentHumidity[4];
  sprintf(currentHumidity, "%d%%", humidity);
  u8g2Fonts.setFont(u8g2_font_fur30_tf);
  u8g2Fonts.setCursor(50, 280);
  u8g2Fonts.print(currentHumidity);

  display.display(true);
}

void displayOut(int temp, int humidity)
{
  Serial.println(F("Display Out"));
  outDisplay = true;
  // Clear zone
  display.fillRect(220, 180, 150, 100, GxEPD_WHITE);
  // Temp
  char currentTemp[5];
  sprintf(currentTemp, "%d°C", temp);
  u8g2Fonts.setFont(u8g2_font_fub42_tf);
  u8g2Fonts.setCursor(230, 230);
  u8g2Fonts.print(currentTemp);
  // Humidity
  char currentHumidity[4];
  sprintf(currentHumidity, "%d%%", humidity);
  u8g2Fonts.setFont(u8g2_font_fur30_tf);
  u8g2Fonts.setCursor(250, 280);
  u8g2Fonts.print(currentHumidity);

  display.display(true);
}

void displayPressure(int pressure)
{
  Serial.println(F("Display pressure"));
  pressureDisplay = true;
  char currentPressure[8];
  sprintf(currentPressure, "%d hPa", pressure);
  display.fillRect(35, 105, 95, 20, GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_fub14_tf);
  u8g2Fonts.setCursor(40, 120);
  u8g2Fonts.print(currentPressure);

  display.display(true);
}

void displayWifi(IPAddress ip)
{
  display.fillScreen(GxEPD_WHITE);
  display.display();
  Serial.println(F("Display Wifi ip"));
  u8g2Fonts.setFont(u8g2_font_fub42_tf);
  u8g2Fonts.setCursor(130, 150);
  u8g2Fonts.print("Wifi");
  u8g2Fonts.setCursor(25, 200);
  u8g2Fonts.print(ip);

  display.display();
}

void displayMoon(uint8_t phase)
{
  Serial.println(F("Display moon"));
  moonDisplay = true;
  display.fillRect(165, 65, 55, 55, GxEPD_WHITE);
  display.drawInvertedBitmap(170, 70, moon_allArray[phase], 48, 48, GxEPD_BLACK);
  display.display(true);
}

void readSensor()
{
  Serial.println("Read sensor");
}

void getTides(HTTPClient &http, WiFiClient &wifi)
{
  Serial.println(F("Get Tides"));
  Serial.println(http.begin(wifi, tideUrl));
  Serial.println(http.GET());
  /*  Serial.println("Get Tides");
   if (WiFi.status() == WL_CONNECTED)
   {
     //  Your Domain name with URL path or IP address with path

     // if (!http.begin(espClientSecure, "tides.lu6fer.fr", 443, "/66", true))
     Serial.print("Connect to tides.lu6fer.fr : ");
     // Serial.println(http.begin(wifiClient, "https://tides.lu6fer.fr"));
     /* if (!http.begin(wifiClient, "https://tides.lu6fer.fr"))
     {
       Serial.println("Failed to connect to server");
     }
     else
     {
       http.addHeader("Content-Type", "application/json");
       http.addHeader("Authorization", "Basic " + tide_auth);
       int httpResponseCode = http.GET();

       if (httpResponseCode > 0)
       {
         Serial.print("HTTP Response code: ");
         Serial.println(httpResponseCode);
         String payload = http.getString();
         Serial.println(payload);
       }
       else
       {
         Serial.print("Error code: ");
         Serial.println(httpResponseCode);
       }
     }
     // http.begin(espClient, "https://tides.lu6fer.fr", 443, "/66", true);
     //  http.begin(url, fingerprint);
     //  http.setAuthorization(serverUser, serverPass);
     //  http.addHeader("Content-Type", "application/json");

     // Send HTTP GET request

     // Free resources
     // http.end();
   } */
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Setup"));
  delay(100);
  display.init(115200);
  Serial.println(F("display init"));

  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  Wifi.addAP(ssid, password);
  Serial.println(F("Wifi"));

  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setRotation(2);
  display.setTextColor(GxEPD_BLACK);
  u8g2Fonts.begin(display);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

  u8g2Fonts.setFont(u8g2_font_fub42_tf);
  u8g2Fonts.setCursor(130, 150);
  u8g2Fonts.print(F("Wifi"));
  u8g2Fonts.setCursor(25, 200);
  u8g2Fonts.print(F("connecting"));
  display.display();

  while (Wifi.run() != WL_CONNECTED && wifiAttempt <= 10)
  {
    delay(1000);
    wifiAttempt++;
    Serial.print(F("."));
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("Connected to: "));
    Serial.println(ssid);
    // Wifi
    WiFiUDP ntpUDP;
    WiFiClient wifiClient;
    //  WiFiClientSecure espClientSecure;
    // WiFiClientSecure wifiSecure;
    // BearSSL::WiFiClientSecure wifiClientBear;
    //  HTTP
    HTTPClient http;
    // MQTT
    // PubSubClient client(wifiClient);
    // NTP
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "fr.pool.ntp.org");

    displayWifi(WiFi.localIP());
    delay(5000);

    drawBackground();

    // getTides(http, wifiClient);
  }
  else
  {
    Serial.print("Wifi connection error: ");
    Serial.println(WiFi.status());
    display.fillScreen(GxEPD_WHITE);
    display.display(false);
    u8g2Fonts.setFont(u8g2_font_fub14_tf);
    u8g2Fonts.setCursor(130, 100);
    u8g2Fonts.print("Wifi");
    u8g2Fonts.setCursor(25, 150);
    u8g2Fonts.print("Connection error");
    u8g2Fonts.setCursor(150, 200);
    switch (WiFi.status())
    {
    case WL_NO_SSID_AVAIL:
      u8g2Fonts.print("No SSID available");
      break;
    case WL_CONNECT_FAILED:
      u8g2Fonts.print("Connection failed");
      break;
    case WL_CONNECTION_LOST:
      u8g2Fonts.print("Connection lost");
      break;
    case WL_DISCONNECTED:
      u8g2Fonts.print("Disconnected");
      break;
    default:
      break;
    }

    display.display(true);
  }
}

void loop()
{
  /* if (Wifi.run() == WL_CONNECTED)
  {
    time(&now);
    localtime_r(&now, &localTime);
    if (!timeDisplay)
    {
      displayDateTime(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min);
    }
    MoonData moon = calculateMoonData(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
    if (!moonDisplay)
    {
      displayMoon(moon.phase);
    }

    // Update every minute
    if (localTime.tm_sec == 0)
    {
      Serial.println(F("Update every minute"));
      displayDateTime(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min);
    }

    // Update every 10 min
    if (localTime.tm_sec == 0 && localTime.tm_min % 10 == 0)
    {
      Serial.println(F("Update every 10 minutes"));
      displayOut(localTime.tm_min, localTime.tm_sec);
      displayIn(localTime.tm_min, localTime.tm_sec);
      displayPressure(localTime.tm_year + 900);
    }

    // Update every day
    if (localTime.tm_hour == 0 && localTime.tm_min == 0)
    {
      Serial.println("Update every day");
      moon = calculateMoonData(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
      displayMoon(moon.phase);
    }
  } */
  // if (WiFi.status() == WL_CONNECTED)
  // {
  //   time(&now); // read the current time
  //   localtime_r(&now, &tm);
  //   MoonData moon = calculateMoonData(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  //   // Init display
  //   if (!timeDisplay)
  //   {
  //     displayDateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  //   }
  //   if (!pressureDisplay)
  //   {
  //     displayPressure(tm.tm_year + 1000);
  //   }
  //   if (!inDisplay)
  //   {
  //     displayIn(tm.tm_min, tm.tm_sec);
  //   }
  //   if (!outDisplay)
  //   {
  //     displayOut(tm.tm_min, tm.tm_sec);
  //   }

  //   if (!tideDisplay)
  //   {
  //     displayTide();
  //   }

  //   if (!moonDisplay)
  //   {
  //     displayMoon(moon.phase);
  //   }

  //   // Update every minute
  //   if (tm.tm_sec == 0)
  //   {
  //     Serial.println("Update every minute");
  //     displayDateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
  //   }

  //   // Update every 10 min
  //   if (tm.tm_sec == 0 && tm.tm_min % 10 == 0)
  //   {
  //     Serial.println("Update every 10 minutes");
  //     displayOut(tm.tm_min, tm.tm_sec);
  //     displayIn(tm.tm_min, tm.tm_sec);
  //     displayPressure(tm.tm_year + 1000);
  //   }

  //   // Update every day
  //   if (tm.tm_hour == 0 && tm.tm_min == 0)
  //   {
  //     Serial.println("Update every day");
  //     moon = calculateMoonData(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  //     displayMoon(moon.phase);
  //   }
  // }
}
