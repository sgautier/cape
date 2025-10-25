#include <Arduino.h>
#include <math.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// --- Pins capteurs ---
#define TEMPERATURE_SENSOR_STOVE               A1
#define TEMPERATURE_SENSOR_BUFFER_TANK_BOTTOM  A2
#define TEMPERATURE_SENSOR_BUFFER_TANK_MIDDLE  A3
#define TEMPERATURE_SENSOR_BUFFER_TANK_TOP     A4

// --- Relais (circulateur) ---
#define RELAY_PIN        2
#define RELAY_ACTIVE_LOW 1   // mets 0 si ton relais est actif HIGH

// --- Seuils & hystérésis ---
#define STOVE_TEMPERATURE_START_CIRCULATOR 33
#define STOVE_TEMPERATURE_HYSTERESIS       10

// --- NTC / filtrage ---
#define TEMPERATURE_NTC_B          3950
#define TEMPERATURE_RESISTOR       100000
#define TEMPERATURE_THERMISTOR     100000
#define TEMPERATURE_NOMINAL        25
#define TEMPERATURE_NB_SAMPLES     8
#define TEMPERATURE_SAMPLE_DELAY   20

// ---------------- Échelle couleurs ------------
#define TEMP_MIN_C   10.0f
#define TEMP_MAX_C   100.0f    // ajuste à 80/90 si tu veux

// Helpers couleurs 16-bits RGB565
#define RGB565(r,g,b)  ( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3) )

static const uint16_t COLOR_DARKGREY = RGB565(64, 64, 64); // gris foncé
//static const uint16_t COLOR_LIGHTGREY = RGB565(200, 200, 200);   // gris clair (optionnel)

// ---------------- TFT & layout ----------------
Adafruit_ST7789 tft = Adafruit_ST7789(10, 9, 8);

#define SCREEN_W 240
#define SCREEN_H 320

// Ballon
#define TANK_X 140
#define TANK_Y 20
#define TANK_W 80
#define TANK_H 300

// Poêle
#define STOVE_X 10
#define STOVE_Y 140
#define STOVE_W 80
#define STOVE_H 180
#define DOOR_W 50
#define DOOR_H 60
#define CHIMNEY_W 20
#define CHIMNEY_H 60
#define CHIMNEY_X (STOVE_X + STOVE_W/2 - CHIMNEY_W/2)
#define CHIMNEY_Y (STOVE_Y - CHIMNEY_H)

// Lignes liaison & circulateur
#define LINE_MID_Y (TANK_Y + TANK_H/2)
#define LINE_BOT_Y (TANK_Y + TANK_H - 10)
#define LINE_X1    (STOVE_X + STOVE_W)
#define LINE_X2    (TANK_X)
#define CIRC_X     ((LINE_X1 + LINE_X2) / 2)
#define CIRC_Y     (LINE_MID_Y)
#define CIRC_R     12

// --- État courant / précédent ---
float stoveTemperature, previousStoveTemperature = NAN;
float bufferTankBottomTemperature, previousBufferTankBottomTemperature = NAN;
float bufferTankMiddleTemperature, previousBufferTankMiddleTemperature = NAN;
float bufferTankTopTemperature, previousBufferTankTopTemperature = NAN;
bool isCirculatorOn, previousIsCirculatorOn = false;

// --- Protos ---
static bool hasChanged(float oldValue, float newValue, float epsilon = 0.1f);

float getSampledTemperature(int sensor);

float getTemperature(int sensor);

void setRelay(bool on);

uint16_t tempToColor565(float t);

void drawStaticUI();

void updateStove();

void updateTank();

void updateCirculator();

// ------------------------------------------------------------------

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false); // au repos

    tft.init(SCREEN_W, SCREEN_H);
    tft.setRotation(0); // 0..3
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextSize(1);

    drawStaticUI();
}

void loop() {
    stoveTemperature = getSampledTemperature(TEMPERATURE_SENSOR_STOVE);
    bufferTankBottomTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_BOTTOM);
    bufferTankMiddleTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_MIDDLE);
    bufferTankTopTemperature = getSampledTemperature(TEMPERATURE_SENSOR_BUFFER_TANK_TOP);

    if (!isCirculatorOn && stoveTemperature >= STOVE_TEMPERATURE_START_CIRCULATOR) {
        isCirculatorOn = true;
    } else if (isCirculatorOn && stoveTemperature <= STOVE_TEMPERATURE_START_CIRCULATOR -
               STOVE_TEMPERATURE_HYSTERESIS) {
        isCirculatorOn = false;
    }
    setRelay(isCirculatorOn);

    updateStove();
    updateTank();
    updateCirculator();
}

// ------------------------------------------------------------------
// Affichage

// =================================================
// ===============   UI RENDU   ====================

void drawStaticUI() {
    // Poêle : corps
    tft.drawRect(STOVE_X, STOVE_Y, STOVE_W, STOVE_H, ST77XX_WHITE);
    // Porte
    int doorX = STOVE_X + (STOVE_W - DOOR_W) / 2;
    int doorY = STOVE_Y + STOVE_H - DOOR_H - 10;
    tft.drawRect(doorX, doorY, DOOR_W, DOOR_H, ST77XX_WHITE);
    // Cheminée
    tft.fillRect(CHIMNEY_X, CHIMNEY_Y, CHIMNEY_W, CHIMNEY_H, ST77XX_WHITE);
    // Label
    tft.setCursor(STOVE_X, STOVE_Y - 12);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print("Poele");

    // Ballon : corps
    tft.drawRect(TANK_X, TANK_Y, TANK_W, TANK_H, ST77XX_WHITE);
    tft.setCursor(TANK_X, TANK_Y - 12);
    tft.print("Ballon tampon");

    // Lignes de liaison
    tft.drawLine(LINE_X1, LINE_MID_Y, LINE_X2, LINE_MID_Y, ST77XX_WHITE);
    tft.drawLine(LINE_X1, LINE_BOT_Y, LINE_X2, LINE_BOT_Y, ST77XX_WHITE);

    // Circulateur (disque + contour)
    tft.drawCircle(CIRC_X, CIRC_Y, CIRC_R, ST77XX_WHITE);
    tft.fillCircle(CIRC_X, CIRC_Y, CIRC_R - 1, COLOR_DARKGREY);
    tft.setCursor(CIRC_X - 8, CIRC_Y - 3);
    tft.setTextColor(ST77XX_WHITE, COLOR_DARKGREY);
    tft.print("OFF");
}

void updateStove() {
    if (hasChanged(stoveTemperature, previousStoveTemperature)) {
        // Remplir l'intérieur (moins 1 px pour garder le bord)
        uint16_t col = tempToColor565(stoveTemperature);
        tft.fillRect(STOVE_X + 1, STOVE_Y + 1, STOVE_W - 2, STOVE_H - 2, col);

        // Re-dessiner la porte par-dessus
        int doorX = STOVE_X + (STOVE_W - DOOR_W) / 2;
        int doorY = STOVE_Y + STOVE_H - DOOR_H - 10;
        tft.drawRect(doorX, doorY, DOOR_W, DOOR_H, ST77XX_WHITE);

        // Afficher la température au centre
        char buf[16];
        dtostrf(stoveTemperature, 5, 1, buf);
        tft.setTextColor(ST77XX_WHITE, col);
        int cx = STOVE_X + 8;
        int cy = STOVE_Y + 8;
        tft.setCursor(cx, cy);
        tft.print(buf);
        tft.print(" C");

        previousStoveTemperature = stoveTemperature;
    }
}

void updateTank() {
    // Découper ballon en 3 zones (haut/milieu/bas)
    int hSec = TANK_H / 3;
    int yTop = TANK_Y;
    int yMid = TANK_Y + hSec;
    int yBot = TANK_Y + 2 * hSec;

    auto drawZone = [&](int y, float tNow, float &tPrev, const char *label) {
        if (hasChanged(tNow, tPrev)) {
            uint16_t col = tempToColor565(tNow);
            tft.fillRect(TANK_X + 1, y + 1, TANK_W - 2, hSec - 2, col);

            // gradient “fake” (petite barre verticale à gauche)
            // tft.drawFastVLine(TANK_X + 2, y + 1, hSec - 2, ST77XX_WHITE);

            // Texte
            char buf[16];
            dtostrf(tNow, 5, 1, buf);
            tft.setTextColor(ST77XX_WHITE, col);
            tft.setCursor(TANK_X + 6, y + 6);
            tft.print(label);
            tft.print(": ");
            tft.print(buf);
            tft.print(" C");

            tPrev = tNow;
        }
    };

    drawZone(yTop, bufferTankTopTemperature, previousBufferTankTopTemperature, "Top");
    drawZone(yMid, bufferTankMiddleTemperature, previousBufferTankMiddleTemperature, "Mid");
    drawZone(yBot, bufferTankBottomTemperature, previousBufferTankBottomTemperature, "Bot");
}

void updateCirculator() {
    if (isCirculatorOn != previousIsCirculatorOn) {
        uint16_t fill = isCirculatorOn ? ST77XX_GREEN : COLOR_DARKGREY;
        uint16_t text = isCirculatorOn ? ST77XX_BLACK : ST77XX_WHITE;
        tft.fillCircle(CIRC_X, CIRC_Y, CIRC_R - 1, fill);
        tft.setCursor(CIRC_X - (isCirculatorOn ? 8 : 10), CIRC_Y - 3);
        tft.setTextColor(text, fill);
        tft.print(isCirculatorOn ? "ON" : "OFF");
        previousIsCirculatorOn = isCirculatorOn;
    }
}


// =================================================
// ===============   Mesures   =====================

float getSampledTemperature(const int sensor) {
    float totalTemperature = 0.0f;
    for (int i = 1; i <= TEMPERATURE_NB_SAMPLES; i++) {
        totalTemperature += getTemperature(sensor);
        delay(TEMPERATURE_SAMPLE_DELAY);
    }
    return totalTemperature / TEMPERATURE_NB_SAMPLES;
}

float getTemperature(const int sensor) {
    int t = analogRead(sensor);
    if (t <= 0) { t = 1; } // évite div/0 si sonde débranchée

    // R_therm = R_fixed / (1023/t - 1)
    float rTherm = (float) TEMPERATURE_RESISTOR / (1023.0f / (float) t - 1.0f);

    // Steinhart-Hart simplifiée (Beta)
    float steinhart = rTherm / (float) TEMPERATURE_THERMISTOR;
    steinhart = logf(steinhart);
    steinhart /= (float) TEMPERATURE_NTC_B;
    steinhart += 1.0f / (TEMPERATURE_NOMINAL + 273.15f);
    steinhart = 1.0f / steinhart;
    steinhart -= 273.15f; // °C

    return steinhart;
}

// =================================================
// ===============   Utils   =======================

// Seuil de rafraîchissement pour éviter de spammer (0.1°C)
static bool hasChanged(const float oldValue, const float newValue, const float epsilon) {
    if (isnan(oldValue) || isnan(newValue)) {
        return true;
    }
    return fabsf(oldValue - newValue) >= epsilon;
}

// Bleu → Jaune → Rouge selon TEMP_MIN_C..TEMP_MAX_C
uint16_t tempToColor565(float t) {
    if (t < TEMP_MIN_C) { t = TEMP_MIN_C; }
    if (t > TEMP_MAX_C) { t = TEMP_MAX_C; }
    float k = (t - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C); // 0..1

    uint8_t r, g, b;
    if (k < 0.5f) {
        // 0..0.5 : bleu (0,0,255) -> jaune (255,255,0)
        float p = k / 0.5f; // 0..1
        r = (uint8_t) (255.0f * p);
        g = (uint8_t) (255.0f * p);
        b = (uint8_t) (255.0f * (1.0f - p));
    } else {
        // 0.5..1 : jaune (255,255,0) -> rouge (255,0,0)
        float p = (k - 0.5f) / 0.5f; // 0..1
        r = 255;
        g = (uint8_t) (255.0f * (1.0f - p));
        b = 0;
    }
    return tft.color565(r, g, b);
}

void setRelay(bool on) {
#if RELAY_ACTIVE_LOW
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
#else
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
#endif
}
