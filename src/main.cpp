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
#define STOVE_TEMPERATURE_START_CIRCULATOR 50
#define STOVE_TEMPERATURE_HYSTERESIS       5

// --- NTC / filtrage ---
#define TEMPERATURE_NTC_B          3950
#define TEMPERATURE_RESISTOR       100000
#define TEMPERATURE_THERMISTOR     100000
#define TEMPERATURE_NOMINAL        25
#define TEMPERATURE_NB_SAMPLES     8
#define TEMPERATURE_SAMPLE_DELAY   20

// ---------------- Échelle couleurs ------------
#define TEMP_MIN_C   10.0f
#define TEMP_MAX_C   85.0f    // ajuste à 80/90 si tu veux

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
#define TANK_Y 70
#define TANK_W 80
#define TANK_H 250

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

// --- Durées relais (ms) ---
unsigned long relayOnStartedAtMs = 0;   // timestamp du dernier ON
unsigned long relayOnElapsedMs   = 0;   // durée courante ON (ms) si ON, sinon 0
unsigned long relayOnTotalMs     = 0;   // cumul des durées ON (ms) sur les sessions terminées

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
void updateStatsOverlay();
static void formatMMSS(unsigned long seconds, char *out, unsigned int n);

// ------------------------------------------------------------------

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false); // au repos

    tft.init(SCREEN_W, SCREEN_H);
    tft.setRotation(0); // 0..3
    tft.fillScreen(ST77XX_WHITE);
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

    // --- Comptage du temps ON du relais (ms) ---
    const unsigned long now = millis();

    // front montant : OFF -> ON
    if (isCirculatorOn && !previousIsCirculatorOn) {
        relayOnStartedAtMs = now;
        relayOnElapsedMs   = 0;
    }

    // front descendant : ON -> OFF
    if (!isCirculatorOn && previousIsCirculatorOn) {
        relayOnTotalMs += (now - relayOnStartedAtMs); // ajoute la session terminée
        relayOnElapsedMs = 0;
    }

    // mise à jour continue quand ON
    if (isCirculatorOn) {
        relayOnElapsedMs = (now - relayOnStartedAtMs);
    } else {
        relayOnElapsedMs = 0;
    }

    updateStove();
    updateTank();
    updateCirculator();
    updateStatsOverlay();
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
    tft.drawRect(doorX, doorY, DOOR_W, DOOR_H, ST77XX_BLACK);
    // Cheminée
    tft.fillRect(CHIMNEY_X, CHIMNEY_Y, CHIMNEY_W, CHIMNEY_H, ST77XX_BLACK);
    // Label
    tft.setCursor(STOVE_X, STOVE_Y - 12);
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.print("Poele");

    // Ballon : corps
    tft.drawRect(TANK_X, TANK_Y, TANK_W, TANK_H, ST77XX_BLACK);
    tft.setCursor(TANK_X, TANK_Y - 12);
    tft.print("Ballon tampon");

    // Lignes de liaison
    tft.drawLine(LINE_X1, LINE_MID_Y, LINE_X2, LINE_MID_Y, ST77XX_BLACK);
    tft.drawLine(LINE_X1, LINE_BOT_Y, LINE_X2, LINE_BOT_Y, ST77XX_BLACK);

    // Circulateur (disque + contour)
    tft.drawCircle(CIRC_X, CIRC_Y, CIRC_R, ST77XX_BLACK);
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
        tft.drawRect(doorX, doorY, DOOR_W, DOOR_H, ST77XX_BLACK);

        // Afficher la température au centre
        char buf[16];
        dtostrf(stoveTemperature, 5, 1, buf);
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
        int cx = STOVE_X + 8;
        int cy = STOVE_Y + 8;
        tft.setCursor(cx, cy);
        tft.print(buf);
        tft.setTextSize(1);

        previousStoveTemperature = stoveTemperature;
    }
}

void updateTank() {
    // Découper ballon en 3 zones (haut/milieu/bas)
    int hSec = TANK_H / 3;
    int yTop = TANK_Y;
    int yMid = TANK_Y + hSec;
    int yBot = TANK_Y + 2 * hSec;

    auto drawZone = [&](int y, float tNow, float &tPrev) {
        if (hasChanged(tNow, tPrev)) {
            uint16_t col = tempToColor565(tNow);
            tft.fillRect(TANK_X + 1, y + 1, TANK_W - 2, hSec - 2, col);

            // Texte avec fond blanc et padding 1 px
            char buf[16];
            dtostrf(tNow, 5, 1, buf);

            const int16_t textX = TANK_X + 6;
            const int16_t textY = y + 30;

            int16_t bx, by;
            uint16_t bw, bh;
            tft.setTextSize(2);
            tft.getTextBounds(buf, textX, textY, &bx, &by, &bw, &bh);
            tft.fillRect(bx - 1, by - 1, bw + 2, bh + 2, ST77XX_WHITE);

            tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
            tft.setCursor(textX, textY);
            tft.print(buf);
            tft.setTextSize(1);

            tPrev = tNow;
        }
    };

    drawZone(yTop, bufferTankTopTemperature, previousBufferTankTopTemperature);
    drawZone(yMid, bufferTankMiddleTemperature, previousBufferTankMiddleTemperature);
    drawZone(yBot, bufferTankBottomTemperature, previousBufferTankBottomTemperature);
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

    uint8_t r = 0, g = 0, b = 0;

    if (k < 0.25f) {
        // Bleu (0,0,255) -> Cyan (0,255,255)
        float p = k / 0.25f;            // 0..1
        r = 0;
        g = (uint8_t)(255.0f * p);
        b = 255;
    } else if (k < 0.50f) {
        // Cyan (0,255,255) -> Vert (0,255,0)
        float p = (k - 0.25f) / 0.25f;  // 0..1
        r = 0;
        g = 255;
        b = (uint8_t)(255.0f * (1.0f - p));
    } else if (k < 0.75f) {
        // Vert (0,255,0) -> Jaune (255,255,0)
        float p = (k - 0.50f) / 0.25f;  // 0..1
        r = (uint8_t)(255.0f * p);
        g = 255;
        b = 0;
    } else {
        // Jaune (255,255,0) -> Rouge (255,0,0)
        float p = (k - 0.75f) / 0.25f;  // 0..1
        r = 255;
        g = (uint8_t)(255.0f * (1.0f - p));
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

static void formatMMSS(unsigned long seconds, char *out, unsigned int n) {
    unsigned long m = seconds / 60UL;
    unsigned long s = seconds % 60UL;
    snprintf(out, n, "%02lu:%02lu", m, s);
}

void updateStatsOverlay() {
    const int x = 4;
    const int y = 4;
    const int lineH = 12;
    const int lines = 3;                 // toujours 3 lignes -> pas d'artefacts
    const int w = 150;                   // un peu plus large pour le pourcentage
    const int h = lines * lineH + 4;

    const unsigned long nowMs = millis();
    const unsigned long uptimeSec = nowMs / 1000UL;

    // temps ON total incluant la session en cours (en ms)
    const unsigned long totalOnMsInclCurrent =
        relayOnTotalMs + (isCirculatorOn ? relayOnElapsedMs : 0UL);

    // pourcentage monotone (en ms, arrondi au plus proche)
    unsigned int pct = 0U;
    if (nowMs > 0UL) {
        pct = (unsigned int)((totalOnMsInclCurrent * 100UL + (nowMs / 2UL)) / nowMs);
        if (pct > 100U) { pct = 100U; }
    }

    // fond blanc couvrant les 3 lignes (efface les anciens contenus)
    tft.fillRect(x - 2, y - 2, w, h, ST77XX_WHITE);

    // texte
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);

    char buf[16];

    // Ligne 1 : Uptime total
    formatMMSS(uptimeSec, buf, sizeof(buf));
    tft.setCursor(x, y);
    tft.print("Up: ");
    tft.print(buf);

    // Ligne 2 : Total ON + %
    formatMMSS((totalOnMsInclCurrent / 1000UL), buf, sizeof(buf));
    tft.setCursor(x, y + lineH);
    tft.print("On: ");
    tft.print(buf);
    tft.print(" (");
    tft.print((int)pct);
    tft.print("%)");

    // Ligne 3 : Session courante (si ON), sinon on laisse blanc (déjà effacé)
    tft.setCursor(x, y + 2 * lineH);
    if (isCirculatorOn) {
        formatMMSS((relayOnElapsedMs / 1000UL), buf, sizeof(buf));
        tft.print("Run: ");
        tft.print(buf);
    }
}
