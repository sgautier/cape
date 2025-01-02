void blink(int onDuration, int offDuration) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(onDuration);
  digitalWrite(LED_BUILTIN, LOW);
  delay(offDuration);
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i = 1; i <= 2; i++) {
    blink(500, 500);
  }
}

// the loop function runs over and over again forever
void loop() {
  blink(125, 125);
}
