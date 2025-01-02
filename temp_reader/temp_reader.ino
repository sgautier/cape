// Source : https://arduino-france.site/thermistance/

#define B 3950 // ntc b constant
#define RESISTOR 100000 // résistance de la résistance, 100 kOhm
#define THERMISTOR 100000 // résistance nominale de la thermistance, 100 kOhm
#define NOMINAL 25 // température nominale

#define sensor A1

#define NB_SAMPLES 10

void setup() {
    Serial.begin(9600);
    pinMode(sensor, INPUT);
}

void loop() {
  float totalTemperature = 0;
  for (int i=1 ; i <= NB_SAMPLES ; i++) {
    totalTemperature += getTemperature();
    delay(200);
  }

  Serial.println(totalTemperature / NB_SAMPLES);
}

float getTemperature() {
  int t = analogRead(sensor);
  float tr = 1023.0 / t - 1;
  tr = RESISTOR / tr;
  float steinhart;
  steinhart = tr / THERMISTOR;
  steinhart = log(steinhart);
  steinhart /= B;
  steinhart += 1.0 / (NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}
