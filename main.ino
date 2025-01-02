#include <Adafruit_GFX.h>  // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#define TEMPERATURE_NTC_B 3950 // ntc b constant
#define TEMPERATURE_RESISTOR 100000 // résistance de la résistance, 100 kOhm
#define TEMPERATURE_THERMISTOR 100000 // résistance nominale de la thermistance, 100 kOhm
#define TEMPERATURE_NOMINAL 25 // température nominale
#define TEMPERATURE_SENSOR A1 // Pin utilisé sur l'Arduino pour relever la température
#define TEMPERATURE_NB_SAMPLES 20 // Number of samples to calculate final temperature value
#define TEMPERATURE_SAMPLE_DELAY 100 // Delay time to wait between temperature samples get value

#define STOVE_TEMPERATURE_START_CIRCULATOR 25

Adafruit_ST7789 tft = Adafruit_ST7789(10, 9, 8);

void setup() {
  pinMode(TEMPERATURE_SENSOR, INPUT);

  tft.init(240, 320);
  tft.setRotation(-1);
}

void loop() {
  float stoveTemperature = getSampledTemperature();
  char stoveStringTemperature[16] = "";
  dtostrf(getSampledTemperature(), 4, 2, stoveStringTemperature );

  char str[32];
  sprintf(str, "Temperature poele : %s deg C", stoveStringTemperature);

  tft.fillScreen(ST77XX_BLACK);

  if (stoveTemperature > STOVE_TEMPERATURE_START_CIRCULATOR) {
      testdrawtext(str, ST77XX_RED);
  } else {
      testdrawtext(str, ST77XX_GREEN);
  }
}

void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

float getSampledTemperature() {
  float totalTemperature = 0;
  for (int i=1 ; i <= TEMPERATURE_NB_SAMPLES ; i++) {
    totalTemperature += getTemperature();
    delay(TEMPERATURE_SAMPLE_DELAY);
  }
  return totalTemperature / TEMPERATURE_NB_SAMPLES;
}

float getTemperature() {
  int t = analogRead(TEMPERATURE_SENSOR);
  float tr = 1023.0 / t - 1;
  tr = TEMPERATURE_RESISTOR / tr;
  float steinhart;
  steinhart = tr / TEMPERATURE_THERMISTOR;
  steinhart = log(steinhart);
  steinhart /= TEMPERATURE_NTC_B;
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}
