#define B 3950 // ntc b constant
#define RESISTOR 100000 // résistance de la résistance, 100 kOhm
#define THERMISTOR 100000 // résistance nominale de la thermistance, 100 kOhm
#define NOMINAL 25 // température nominale
 
#define sensor A1
 
void setup() {
    Serial.begin(9600);
    pinMode(sensor, INPUT);
}
 
void loop() {
    int t = analogRead(sensor);
    float tr = 1023.0 / t - 1;
    tr = RESISTOR / tr;
    Serial.print("R=");
    Serial.print(tr);
    Serial.print(", t=");
 
    float steinhart;
    steinhart = tr / THERMISTOR;
    steinhart = log(steinhart);
    steinhart /= B;
    steinhart += 1.0 / (NOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15; 
    Serial.println(steinhart);
  
    delay(100);
}
