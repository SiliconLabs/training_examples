/* 
 Check whether button is held down for 5 seconds

 Button must be down when entering the function

 During the timer the LED will flash rapidly

 When held down for the period the LED is lit until the button is released

 LED is turned off upon exiting

 Returns true if button was held down
 */

#define BTN_ACTIVE LOW        // State of button when pressed
#define BUTTON_HELD_SECONDS 5 // Time button must be held to return true (s)

bool button_held() {
  bool btn_state;        // Button state
  bool btn_held = false; // Button held
  uint32_t now_millis;   // For timers
  uint32_t btn_millis;   // For timers
  uint32_t held_millis;  // For timers

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_BUILTIN_INACTIVE);
  // Initialise and read on-board button
  pinMode(BTN_BUILTIN, INPUT_PULLUP);
  btn_state = digitalRead(BTN_BUILTIN);
  // Button is down ?
  if (btn_state == BTN_ACTIVE) {
    Serial.printf("Button down, starting %d second timer...\n", BUTTON_HELD_SECONDS);
    // Start timer
    held_millis = BUTTON_HELD_SECONDS * 1000; 
    now_millis = btn_millis = millis();
    // Wait for button to be released or timer to expire
    while(btn_state == BTN_ACTIVE && now_millis - btn_millis < held_millis) {
      // Update button and timer
      btn_state = digitalRead(BTN_BUILTIN);
      now_millis = millis();
      // Flash LED (64ms intervals)
      if (now_millis & 0x40) digitalWrite(LED_BUILTIN, LED_BUILTIN_INACTIVE);
      else                   digitalWrite(LED_BUILTIN, LED_BUILTIN_ACTIVE);
    }
    // Button is still pressed ?
    if (btn_state == BTN_ACTIVE) {
      // Note button was held
      btn_held = true;
      // Turn on LED
      digitalWrite(LED_BUILTIN, LED_BUILTIN_ACTIVE);      
      Serial.println("Button held, release to continue...");
      // Wait for button to be released
      while (digitalRead(BTN_BUILTIN) == BTN_ACTIVE);
      Serial.println("Button held then released");
    }
    // Button no longer pressed ?
    else {
      Serial.println("Button released");
    }
    // Make sure LED is off
    digitalWrite(LED_BUILTIN, LED_BUILTIN_INACTIVE);
  }

  return btn_held;  
}