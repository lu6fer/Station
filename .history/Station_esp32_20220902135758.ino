// Display
#define ENABLE_GxEPD2_GFX 1
#ifndef RST_PIN
#define RST_PIN 16
#define DC_PIN 17
#define CS_PIN SS
#define BUSY_PIN 4
#endif

#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=5*/ CS_PIN, /*DC=*/DC_PIN, /*RST=*/RST_PIN, /*BUSY=*/BUSY_PIN)); // GDEW042T2
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

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

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup");
  display.init();
  helloWorld();
  display.hibernate();
}

const char HelloWorld[] = "Hello World!";

void helloWorld()
{
  display.setRotation(2);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(x, y);
  display.print(HelloWorld);
  display.display();
}

void loop(){};
