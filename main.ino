#include <Arduino.h>
#include <Adafruit_GFX.h>  // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#define TEMPERATURE_SENSOR_STOVE A1 // Pin utilisé sur l'Arduino pour relever la température du poêle
#define TEMPERATURE_SENSOR_BUFFER_TANK_BOTTOM A2 // Pin utilisé sur l'Arduino pour relever la température du bas du ballon
#define TEMPERATURE_SENSOR_BUFFER_TANK_MIDDLE A3 // Pin utilisé sur l'Arduino pour relever la température du milieu du ballon
#define TEMPERATURE_SENSOR_BUFFER_TANK_TOP A4 // Pin utilisé sur l'Arduino pour relever la température du haut du ballon

#define STOVE_TEMPERATURE_START_CIRCULATOR 25
#define STOVE_TEMPERATURE_HYSTERESIS 10

#define TEMPERATURE_NTC_B 3950 // ntc b constant
#define TEMPERATURE_RESISTOR 100000 // résistance de la résistance, 100 kOhm
#define TEMPERATURE_THERMISTOR 100000 // résistance nominale de la thermistance, 100 kOhm
#define TEMPERATURE_NOMINAL 25 // température nominale
#define TEMPERATURE_NB_SAMPLES 20 // Number of samples to calculate final temperature value
#define TEMPERATURE_SAMPLE_DELAY 100 // Delay time to wait between temperature samples get value

Adafruit_ST7789 tft = Adafruit_ST7789(10, 9, 8);
float stoveTemperature;
float bufferTankBottomTemperature;
float bufferTankMiddleTemperature;
float bufferTankTopTemperature;
bool isCirculatorStarted;

// --- Prototypes (déclarations) ---
void testdrawtext(const char *text, uint16_t color);

float getSampledTemperature(int sensor);

float getTemperature(int sensor);

void refreshScreen();

void setup() {
    pinMode(TEMPERATURE_SENSOR_STOVE, INPUT);

    tft.init(240, 320);
    tft.setRotation(-1);
}

void loop() {
    stoveTemperature = getSampledTemperature(TEMPERATURE_SENSOR_STOVE);
    bufferTankBottomTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_BOTTOM);
    bufferTankMiddleTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_MIDDLE);
    bufferTankTopTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_TOP);

    isCirculatorStarted = false;
    if (stoveTemperature > STOVE_TEMPERATURE_START_CIRCULATOR) {
        // Démarrer le circulateur
        isCirculatorStarted = true;
    } else if (stoveTemperature < STOVE_TEMPERATURE_START_CIRCULATOR - STOVE_TEMPERATURE_HYSTERESIS) {
        // Stopper le circulateur si la température passe sous la température de démarrage avec application de l'hystérésis
    }

    refreshScreen();
}

void refreshScreen() {
    char stoveStringTemperature[16] = "";
    dtostrf(stoveTemperature, 4, 2, stoveStringTemperature);

    char str[32];
    sprintf(str, "Temperature poele : %s deg C", stoveStringTemperature);

    tft.fillScreen(ST77XX_BLACK);

    if (isCirculatorStarted) {
        testdrawtext(str, ST77XX_RED);
    } else {
        testdrawtext(str, ST77XX_GREEN);
    }
}

void testdrawtext(const char *text, uint16_t color) {
    tft.setCursor(0, 0);
    tft.setTextColor(color);
    tft.setTextWrap(true);
    tft.print(text);
}

float getSampledTemperature(const int sensor) {
    float totalTemperature = 0;
    for (int i = 1; i <= TEMPERATURE_NB_SAMPLES; i++) {
        totalTemperature += getTemperature(sensor);
        delay(TEMPERATURE_SAMPLE_DELAY);
    }
    return totalTemperature / TEMPERATURE_NB_SAMPLES;
}

float getTemperature(const int sensor) {
    int t = analogRead(sensor);
    float tr = 1023.0 / t - 1;
    tr = TEMPERATURE_RESISTOR / tr;
    float steinhart = tr / TEMPERATURE_THERMISTOR;
    steinhart = log(steinhart);
    steinhart /= TEMPERATURE_NTC_B;
    steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;
    return steinhart;
}
