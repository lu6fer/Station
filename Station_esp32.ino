// Display
#define ENABLE_GxEPD2_GFX 0
#ifndef RST_PIN
#define RST_PIN 16
#define DC_PIN 17
#define CS_PIN SS
#define BUSY_PIN 4
#endif

/* moon: pi/180 */
#ifndef RPD
#define RPD 1.74532925199e-2
#endif

// Wifi
#ifndef STASSID
#define STASSID "Chez_FloEtFlo"
#define STAPSK "Vyygop8j!"
//#define STASSID "Dialler"
//#define STAPSK "123456789"
#endif

// BME280
#ifndef BME280_ADDR
#define BME280_ADDR 0x76
#endif

#ifndef MQTT_SERVER
#define MQTT_SERVER "192.168.0.253"
#define MQTT_ID "Station"
#define MQTT_USER "station"
#define MQTT_PASS "station"
#endif

// Display
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// Wifi
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// NTP
#include <time.h>

// BME280
#include <Wire.h>
#include <Adafruit_BME280.h>

// MQTT
#include <PubSubClient.h>

#include "moon.h"
#include "icons.h"
#include "rootCa.h"

// Display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=5*/ CS_PIN, /*DC=*/DC_PIN, /*RST=*/RST_PIN, /*BUSY=*/BUSY_PIN)); // GDEW042T2
U8G2_FOR_ADAFRUIT_GFX u8g2;
// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Tide
StaticJsonDocument<768> tidesJson;

// Moon
typedef struct MoonData
{
  uint8_t phase;
  double illumination;
} MoonData;
MoonData currentMoon;

// Wifi
int wifiAttempt = 0;

// NTP
time_t now;   // this is the epoch
tm localTime; // the structure tm holds time information in a more convient way

// BME280
Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

// MQTT
const char *mqtt_server PROGMEM = MQTT_SERVER;

// Display
bool timeDisplay = false;
bool dateDisplay = false;
bool pressureDisplay = false;
bool tideDisplay = false;
bool inTempDisplay = false;
bool outTempDisplay = false;
bool inHumDisplay = false;
bool outHumDisplay = false;
bool moonDisplay = false;
uint16_t currentDay = 0;

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
 * Wifi Reconnect
 */
void wifiReconnect(WiFiEvent_t event, WiFiEventInfo_t info)
{
  const char ssid[] PROGMEM = STASSID;
  const char password[] PROGMEM = STAPSK;
  Serial.printf("Connecting to: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && wifiAttempt <= 10)
  {
    delay(500);
    Serial.print(".");
    wifiAttempt++;
  }
  Serial.println("");
}

/*
 * Wifi setup
 */
bool wifiSetup()
{
  WiFi.onEvent(wifiReconnect, SYSTEM_EVENT_STA_DISCONNECTED);
  const char ssid[] PROGMEM = STASSID;
  const char password[] PROGMEM = STAPSK;
  Serial.printf("Connecting to: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && wifiAttempt <= 10)
  {
    delay(500);
    Serial.print(".");
    wifiAttempt++;
  }
  Serial.println("");

  display.setPartialWindow(16, 16, 370, 120);

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("WiFi connected"));
    Serial.println(WiFi.localIP());

    int16_t tw, ta, td, th;
    uint16_t ssid_x, ssid_y, ip_x, ip_y;
    //
    u8g2.setFont(u8g2_font_fub14_tf);
    tw = u8g2.getUTF8Width(ssid); // text box width
    ta = u8g2.getFontAscent();    // positive
    td = u8g2.getFontDescent();   // negative; in mathematicians view
    th = ta - td;                 // text box height
    ssid_x = ((370 - tw) / 2) + 16;
    ssid_y = (display.height() - th) / 2 + ta;

    const String ip = WiFi.localIP().toString();
    tw = u8g2.getUTF8Width(ip.c_str()); // text box width
    ta = u8g2.getFontAscent();          // positive
    td = u8g2.getFontDescent();         // negative; in mathematicians view
    th = ta - td;                       // text box height
    ip_x = ((370 - tw) / 2) + 16;
    ip_y = (display.height() - th) / 2 + ta;

    display.firstPage();
    do
    {
      u8g2.setCursor(ssid_x, 70);
      u8g2.print(ssid);
      u8g2.setCursor(ip_x, 100);
      u8g2.print(ip);
    } while (display.nextPage());

    delay(1000);

    display.firstPage();
    do
    {
      display.fillRect(16, 16, 370, 120, GxEPD_WHITE);
    } while (display.nextPage());
  }
  else
  {
    Serial.println(F("WiFi not connected"));
    int16_t tbx, tby;
    uint16_t tbw, tbh, ssid_x, ssid_y, err_x, err_y;

    // display.setPartialWindow(16, 16, 380, 120);

    u8g2.setFont(u8g2_font_fub14_tf);

    // SSID
    display.getTextBounds(ssid, 0, 0, &tbx, &tby, &tbw, &tbh);
    ssid_x = ((380 - tbw) / 2) - tbx;
    ssid_y = ((120 - tbh - 20) / 2) - tby;
    // Ip address
    const char errorMsg[] PROGMEM = "Connection error";
    display.getTextBounds(errorMsg, 0, 0, &tbx, &tby, &tbw, &tbh);
    err_x = ((380 - tbw) / 2) - tbx;
    err_y = ((120 - tbh + 20) / 2) - tby;

    display.firstPage();
    do
    {
      u8g2.setCursor(ssid_x, ssid_y);
      u8g2.print(ssid);
      u8g2.setCursor(err_x, err_y);
      u8g2.print(errorMsg);
    } while (display.nextPage());
  }

  return WiFi.status() == WL_CONNECTED;
}

/*
 * MQTT
 */

void mqttSetup(int sec)
{
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setKeepAlive(20);
  mqttClient.setSocketTimeout(5);
  mqttClient.setCallback(mqttCallback);
  if (!mqttClient.connected() && sec % 5 == 0)
  {
    Serial.println("Mqtt not connected, retry every 5s");
    if (mqttClient.connect(MQTT_ID, MQTT_USER, MQTT_PASS))
    {
      Serial.println("Mqtt connected");
      mqttClient.subscribe("home/out/temperature");
      mqttClient.subscribe("home/out/humidity");
    }
  }
  else
  {
    mqttClient.loop();
  }
}

void mqttCallback(char *topic, byte *message, unsigned int length)
{
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    messageTemp += (char)message[i];
  }

  Serial.printf("[%s]: %s\n", topic, messageTemp);

  if (String(topic) == "home/out/temperature")
  {
    displayOutTemperature(messageTemp.toFloat());
  }

  if (String(topic) == "home/out/humidity")
  {
    displayOutHumidity(messageTemp.toFloat());
  }
}

void mqttPublishSensor()
{
  sensors_event_t temp_event, humidity_event, pressure_event;
  bme_temp->getEvent(&temp_event);
  bme_humidity->getEvent(&humidity_event);
  bme_pressure->getEvent(&pressure_event);

  char temp[6];
  sprintf(temp, "%.1f", temp_event.temperature);
  char humidity[6];
  sprintf(humidity, "%.1f", humidity_event.relative_humidity);
  char pressure[7];
  sprintf(pressure, "%d", (int)pressure_event.pressure);
  mqttClient.publish("home/station/temperature", temp);
  mqttClient.publish("home/station/humidity", humidity);
  mqttClient.publish("home/station/pressure", pressure);
}

/*
 * Get tides
 */
bool getTides()
{
  WiFiClientSecure *client = new WiFiClientSecure;
  if (client)
  {
    client->setCACert(rootCACertificate);
    {
      HTTPClient https;
      if (https.begin(*client, "https://tides.lu6fer.fr/66"))
      {
        https.setAuthorization("tide", "tideScraper");
        https.addHeader("Content-Type", "application/json");
        int httpCode = https.GET();
        if (httpCode > 0)
        {
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
          {
            String payload = https.getString();
            DeserializationError error = deserializeJson(tidesJson, payload);
            if (error)
            {
              Serial.print("deserializeJson() failed: ");
              Serial.println(error.c_str());
              return false;
            }
          }
        }
        https.end();
      }
    }
    delete client;
    return true;
  }
}

/*
 * Display
 */

void drawBackground()
{
  Serial.println(F("Draw background"));
  display.init(0, false, 2, false);
  u8g2.begin(display);
  //  display.setTextColor(GxEPD_BLACK);
  display.setRotation(0);

  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.display();
  u8g2.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2.setFontDirection(0);             // left to right (this is default)
  u8g2.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color

  display.firstPage();
  int thick = 3;
  int radius = 10;
  do
  {
    for (int i = 0; i < thick; i++)
    {
      // Top box
      display.drawRoundRect(5 + i, 5 + i, 390 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);

      // Right box
      display.drawRoundRect(5 + i, 155 + i, 190 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);

      // Left box
      display.drawRoundRect(205 + i, 155 + i, 190 - i * 2, 140 - i * 2, radius - i * 2, GxEPD_BLACK);
    }
    display.drawInvertedBitmap(15, 165, icon_home, 16, 16, GxEPD_BLACK);
    display.drawInvertedBitmap(215, 165, icon_out, 16, 16, GxEPD_BLACK);
  } while (display.nextPage());
  // display.display(true);
}

void displayTime(int hour = 0, int min = 0)
{
  Serial.println(F("Display time"));
  timeDisplay = true;

  int16_t tw, ta, td, th;
  uint16_t hour_x, hour_y;

  // Time
  char currentHour[6];
  sprintf(currentHour, "%02d:%02d", hour, min);

  u8g2.setFont(u8g2_font_fub42_tf);
  tw = u8g2.getUTF8Width(currentHour); // text box width
  ta = u8g2.getFontAscent();           // positive
  td = u8g2.getFontDescent();          // negative; in mathematicians view
  th = ta - td;                        // text box height
  hour_x = ((155 - tw) / 2) + 10;
  hour_y = (display.height() - th) / 2 + ta;
  // display.setPartialWindow(10, 15, tw + 5, th + 5);
  display.setPartialWindow(hour_x, 70 - th, tw + 5, th + 5);
  display.firstPage();
  do
  {
    display.fillRect(hour_x, 70 - th, tw + 5, th + 5, GxEPD_WHITE);
    u8g2.setCursor(hour_x, 70);
    u8g2.print(currentHour);
  } while (display.nextPage());
}

void displayDate(int year = 0, int month = 0, int day = 0)
{
  Serial.println(F("Display date"));
  dateDisplay = true;
  currentDay = localTime.tm_mday;

  int16_t tw, ta, td, th;
  uint16_t date_x, date_y;

  // Date
  char currentDate[11];
  sprintf(currentDate, "%02d/%02d/%d", day, month, year);

  u8g2.setFont(u8g2_font_fub14_tf);
  tw = u8g2.getUTF8Width(currentDate); // text box width
  ta = u8g2.getFontAscent();           // positive
  td = u8g2.getFontDescent();          // negative; in mathematicians view
  th = ta - td;                        // text box height
  date_x = ((155 - tw) / 2) + 10;
  date_y = (display.height() - th) / 2 + ta;
  display.setPartialWindow(date_x - 2, 93 - th, tw + 2, th + 2);

  display.firstPage();
  do
  {
    display.fillRect(date_x - 2, 93 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(date_x, 95);
    u8g2.print(currentDate);
  } while (display.nextPage());
}

void displayInTemperature()
{
  inTempDisplay = true;

  sensors_event_t temp_event;
  bme_temp->getEvent(&temp_event);
  Serial.print(F("Display In temperature: "));
  Serial.println(temp_event.temperature);

  int16_t tw, ta, td, th;
  uint16_t x, y;

  char currentTemp[7];
  // sprintf(currentTemp, "%f", temp_event.temperature);
  // mqttClient.publish("home/station/temperature", currentTemp);
  //  Temp

  sprintf(currentTemp, "%.1f°C", temp_event.temperature);
  u8g2.setFont(u8g2_font_fub42_tf);
  tw = u8g2.getUTF8Width(currentTemp); // text box width
  ta = u8g2.getFontAscent();           // positive
  td = u8g2.getFontDescent();          // negative; in mathematicians view
  th = ta - td;                        // text box height

  x = ((160 - tw) / 2) + 20;
  y = (display.height() - th) / 2 + ta;
  display.setPartialWindow(x - 2, 235 - th, tw + 2, th + 2);

  display.firstPage();
  do
  {
    display.fillRect(x - 2, 235 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(x, 230);
    u8g2.print(currentTemp);
  } while (display.nextPage());
}

void displayInHumidity()
{
  inHumDisplay = true;

  sensors_event_t humidity_event;
  bme_humidity->getEvent(&humidity_event);
  Serial.print(F("Display In humidity: "));
  Serial.println(humidity_event.relative_humidity);

  display.fillRect(20, 185, 165, 100, GxEPD_WHITE);
  int16_t tw, ta, td, th;
  uint16_t x, y;

  char currentHumidity[8];
  // sprintf(currentHumidity, "%f", humidity_event.relative_humidity);
  // mqttClient.publish("home/station/humidity", currentHumidity);
  //  Humidity
  sprintf(currentHumidity, "%d%%", (int)humidity_event.relative_humidity);
  u8g2.setFont(u8g2_font_fur30_tf);
  tw = u8g2.getUTF8Width(currentHumidity); // text box width
  ta = u8g2.getFontAscent();               // positive
  td = u8g2.getFontDescent();              // negative; in mathematicians view
  th = ta - td;                            // text box height

  x = ((160 - tw) / 2) + 20;
  y = (display.height() - th) / 2 + ta;

  display.setPartialWindow(x - 2, 280 - th, tw + 2, th + 2);
  display.firstPage();
  do
  {
    display.fillRect(x - 2, 280 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(x, 280);
    u8g2.print(currentHumidity);
  } while (display.nextPage());
}

void displayOutTemperature(float temp)
{
  Serial.print(F("Display Out temperature: "));
  Serial.println(temp);
  outTempDisplay = true;

  int16_t tw, ta, td, th;
  uint16_t x, y;

  // Temp
  char currentTemp[7];
  sprintf(currentTemp, "%.1f°C", temp);
  u8g2.setFont(u8g2_font_fub42_tf);
  tw = u8g2.getUTF8Width(currentTemp); // text box width
  ta = u8g2.getFontAscent();           // positive
  td = u8g2.getFontDescent();          // negative; in mathematicians view
  th = ta - td;                        // text box height

  x = ((160 - tw) / 2) + 220;
  y = (display.height() - th) / 2 + ta;

  display.setPartialWindow(x - 2, 235 - th, tw + 2, th + 2);
  display.firstPage();
  do
  {
    display.fillRect(x - 2, 235 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(x, 230);
    u8g2.print(currentTemp);
  } while (display.nextPage());
}

void displayOutHumidity(float humidity)
{
  Serial.print(F("Display Out humidity: "));
  Serial.println(humidity);
  outHumDisplay = true;

  int16_t tw, ta, td, th;
  uint16_t x, y;
  // Humidity
  char currentHumidity[5];
  sprintf(currentHumidity, "%d%%", (int)humidity);
  u8g2.setFont(u8g2_font_fur30_tf);
  tw = u8g2.getUTF8Width(currentHumidity); // text box width
  ta = u8g2.getFontAscent();               // positive
  td = u8g2.getFontDescent();              // negative; in mathematicians view
  th = ta - td;                            // text box height

  x = ((160 - tw) / 2) + 220;
  y = (display.height() - th) / 2 + ta;

  display.setPartialWindow(x - 2, 280 - th, tw + 2, th + 2);
  display.firstPage();
  do
  {
    display.fillRect(x - 2, 280 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(x, 280);
    u8g2.print(currentHumidity);
  } while (display.nextPage());
}

void displayPressure()
{
  pressureDisplay = true;

  sensors_event_t pressure_event;
  bme_pressure->getEvent(&pressure_event);
  Serial.print(F("Display pressure: "));
  Serial.println(pressure_event.pressure);

  int16_t tw, ta, td, th;
  uint16_t x, y;

  char currentPressure[10];
  // sprintf(currentPressure, "%f", pressure_event.pressure);
  // mqttClient.publish("station/pressure", currentPressure);

  sprintf(currentPressure, "%d hPa", (int)pressure_event.pressure);

  u8g2.setFont(u8g2_font_fub17_tf);
  tw = u8g2.getUTF8Width(currentPressure); // text box width
  ta = u8g2.getFontAscent();               // positive
  td = u8g2.getFontDescent();              // negative; in mathematicians view
  th = ta - td;                            // text box height

  x = ((95 - tw) / 2) + 40;
  y = (display.height() - th) / 2 + ta;

  display.setPartialWindow(x - 2, 130 - th, tw + 2, th + 2);
  display.firstPage();
  do
  {
    display.fillRect(x - 2, 280 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(x, 130);
    u8g2.print(currentPressure);
  } while (display.nextPage());
}

void displayMoon(MoonData moon)
{
  Serial.println(F("Display moon"));
  moonDisplay = true;
  int16_t tw, ta, td, th;
  uint16_t x, y;

  char illumination[6];
  int ill_percent = moon.illumination * 100;
  sprintf(illumination, "%d%%", ill_percent);
  u8g2.setFont(u8g2_font_fub14_tf);
  tw = u8g2.getUTF8Width(illumination); // text box width
  ta = u8g2.getFontAscent();            // positive
  td = u8g2.getFontDescent();           // negative; in mathematicians view
  th = ta - td;                         // text box height

  x = ((65 - tw) / 2) + 165;

  display.setPartialWindow(170, 65, 60, 70);
  display.firstPage();
  do
  {
    display.fillRect(170, 65, 60, 70, GxEPD_WHITE);
    u8g2.setCursor(x, 135);
    u8g2.print(illumination);
    display.drawInvertedBitmap(170, 70, moon_allArray[moon.phase], 48, 48, GxEPD_BLACK);
  } while (display.nextPage());
}

void displayTide()
{
  tideDisplay = true;
  const char *harbor = tidesJson["harbor"]["name"];
  JsonArray tides = tidesJson["tides"];
  const char bm[] PROGMEM = "BM";
  const char pm[] PROGMEM = "PM";
  const char coef[] PROGMEM = "Coef";
  Serial.print("Display Tide: ");
  Serial.println(harbor);

  int16_t tw, ta, td, th;
  uint16_t harbor_x, bm_x, pm_x, coef_x,
      bm_hour_x, pm_hour_x, coef_hour_x;

  // Harbor
  u8g2.setFont(u8g2_font_fub17_tf);
  tw = u8g2.getUTF8Width(harbor); // text box width
  ta = u8g2.getFontAscent();      // positive
  td = u8g2.getFontDescent();     // negative; in mathematicians view
  th = ta - td;                   // text box height
  harbor_x = ((155 - tw) / 2) + 220;

  display.setPartialWindow(harbor_x - 2, 40 - th, tw + 2, th + 2);
  display.firstPage();
  do
  {
    display.fillRect(harbor_x - 2, 40 - th, tw + 2, th + 2, GxEPD_WHITE);
    u8g2.setCursor(harbor_x, 40);
    u8g2.print(harbor);
  } while (display.nextPage());

  // BM title
  u8g2.setFont(u8g2_font_fub11_tf);
  tw = u8g2.getUTF8Width(bm); // text box width
  ta = u8g2.getFontAscent();  // positive
  td = u8g2.getFontDescent(); // negative; in mathematicians view
  th = ta - td;               // text box height
  bm_x = (tw / 2) + 220;
  // PM title
  tw = u8g2.getUTF8Width(pm); // text box width
  ta = u8g2.getFontAscent();  // positive
  td = u8g2.getFontDescent(); // negative; in mathematicians view
  th = ta - td;               // text box height
  pm_x = ((155 - tw) / 2) + 235;
  // coef title
  tw = u8g2.getUTF8Width(coef); // text box width
  ta = u8g2.getFontAscent();    // positive
  td = u8g2.getFontDescent();   // negative; in mathematicians view
  th = ta - td;                 // text box height
  coef_x = (155 - tw) + 220;

  display.setPartialWindow(bm_x - 2, 60 - th, 155, 20);
  display.firstPage();
  do
  {
    display.fillRect(bm_x - 2, 60 - th, 155, 20, GxEPD_WHITE);
    u8g2.setCursor(bm_x, 60);
    u8g2.print(bm);
    u8g2.setCursor(pm_x, 60);
    u8g2.print(pm);
    u8g2.setCursor(coef_x, 60);
    u8g2.print(coef);
  } while (display.nextPage());

  for (int i = 0; i < tides.size(); i++)
  {
    JsonObject tide = tides[i];
    uint16_t y;
    if (tide["type"] == "BM")
    {
      const char *bm_hour = tide["hour"];
      // Serial.printf("%s ", bm_hour);
      tw = u8g2.getUTF8Width(bm_hour); // text box width
      ta = u8g2.getFontAscent();       // positive
      td = u8g2.getFontDescent();      // negative; in mathematicians view
      th = ta - td;                    // text box height
      bm_hour_x = (tw / 2) + 210;
      y = (i * 10) + 80;
      display.setPartialWindow(bm_x - 2, y - th, 155, 20);
      display.firstPage();
      do
      {
        display.fillRect(bm_x - 2, y - th, 155, 20, GxEPD_WHITE);
        u8g2.setCursor(bm_hour_x, y);
        u8g2.print(bm_hour);
      } while (display.nextPage());
    }
    else if (tide["type"] == "PM")
    {
      const char *pm_hour = tide["hour"];
      const char *coef_hour = tide["coef"];
      y = ((i - 1) * 10) + 80;
      // Serial.printf("%s %s \n", pm_hour, coef_hour);
      tw = u8g2.getUTF8Width(pm_hour); // text box width
      ta = u8g2.getFontAscent();       // positive
      td = u8g2.getFontDescent();      // negative; in mathematicians view
      th = ta - td;                    // text box height
      pm_hour_x = ((155 - tw) / 2) + 235;

      int16_t total_tw = tw;
      tw = u8g2.getUTF8Width(coef_hour); // text box width
      ta = u8g2.getFontAscent();         // positive
      td = u8g2.getFontDescent();        // negative; in mathematicians view
      th = ta - td;                      // text box height
      coef_hour_x = (155 - tw) + 220;

      total_tw += tw;
      display.setPartialWindow(pm_hour_x, y - th, 100, th);
      display.firstPage();
      do
      {
        display.fillRect(pm_hour_x, y - th, 100, th, GxEPD_WHITE);
        u8g2.setCursor(pm_hour_x, y);
        u8g2.print(pm_hour);
        u8g2.setCursor(coef_hour_x, y);
        u8g2.print(coef_hour);
      } while (display.nextPage());
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup");
  // Display setup
  drawBackground();

  if (!bme.begin(BME280_ADDR))
  {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    while (1)
      delay(10);
  }
  bme.setTemperatureCompensation(-2.0);

  Serial.printf("compensation: %.1f", bme.getTemperatureCompensation());

  if (wifiSetup())
  {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "fr.pool.ntp.org");
    getTides();
  }
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&localTime))
  {
    mqttSetup(localTime.tm_sec);

    // initial display
    if (!timeDisplay)
    {
      displayTime(localTime.tm_hour, localTime.tm_min);
    }
    if (!dateDisplay)
    {
      displayDate(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
    }
    if (!inTempDisplay)
    {
      displayInTemperature();
    }
    if (!inHumDisplay)
    {
      displayInHumidity();
    }
    if (!pressureDisplay)
    {
      displayPressure();
    }
    if (!moonDisplay)
    {
      currentMoon = calculateMoonData(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
      displayMoon(currentMoon);
    }

    if (!tideDisplay)
    {
      displayTide();
    }

    // Update
    if (localTime.tm_sec == 0)
    {
      Serial.println(F("Update every minute"));
      displayTime(localTime.tm_hour, localTime.tm_min);
    }

    if (localTime.tm_sec == 0 && localTime.tm_min % 10 == 0 && localTime.tm_sec == 0)
    {
      Serial.println(F("Update every 10 minutes"));
      displayInTemperature();
      displayInHumidity();
      displayPressure();
      mqttPublishSensor();
    }

    if (currentDay != localTime.tm_mday)
    {
      Serial.println("Update every day");
      if (getTides())
      {
        displayDate(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
        currentMoon = calculateMoonData(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
        displayMoon(currentMoon);
        displayTide();
      }
      display.refresh();
    }
  }
  else
  {
    wifiSetup();
  }
  display.powerOff();
};
