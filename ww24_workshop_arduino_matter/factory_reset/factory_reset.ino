void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("button_held_test");
  // Has button been held at start up ?
  if (button_held()) {
    Serial.println("Factory reset");
    // Do factory reset by erasing non-volatile memory
    nvm3_eraseAll(nvm3_defaultHandle);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
