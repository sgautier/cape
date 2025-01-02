// Source : https://arduino-france.site/thermistance/

#define TEMPERATURE_NTC_B 3950 // ntc b constant
#define TEMPERATURE_RESISTOR 100000 // résistance de la résistance, 100 kOhm
#define TEMPERATURE_THERMISTOR 100000 // résistance nominale de la thermistance, 100 kOhm
#define TEMPERATURE_NOMINAL 25 // température nominale
#define TEMPERATURE_SENSOR A1 // Pin utilisé sur l'Arduino pour relever la température
#define TEMPERATURE_NB_SAMPLES 20 // Number of samples to calculate final temperature value
#define TEMPERATURE_SAMPLE_DELAY 100 // Delay time to wait between temperature samples get value

void setup() {
    Serial.begin(9600);
    pinMode(TEMPERATURE_SENSOR, INPUT);
}

void loop() {
  Serial.println(getSampledTemperature());
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
