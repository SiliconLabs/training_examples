/*
   Matter ARGB Fan

   The example shows how to create a fan device with the Arduino Matter API.

   The example lets users control the fan through Matter and displays the 
   values set by the user.
   
   The device has to be commissioned to a Matter hub first.

   This enhanced version controls and monitors a 4 wire fan, displaying the 
   fan's state, speed percentage and RPM reading on an OLED display. It also 
   controls ARGB lights on a gaming fan, rotating them relative to the fan's 
   speed and setting their color according to the fan's state.

   Compatible boards:
   - Arduino Nano Matter
   - SparkFun Thing Plus MGM240P
   - xG24 Explorer Kit
   - xG24 Dev Kit
   - Seeed Studio XIAO MG24 (Sense)

   Other hardware:
   - Adafruit Monochrome 1.12" 128x128 OLED Graphic Display - STEMMA QT / Qwiic
     https://www.adafruit.com/product/5297
   - 4-channel I2C-safe Bi-directional Logic Level Converter - BSS138
     https://www.adafruit.com/product/757
   - Artic P12 PWM PST A-RGB Fan 
     https://www.arctic.de/en/P12-PWM-PST-A-RGB/P12A-RGB0dB  
   - Cooler Master MasterFan MF120 Halo2
     https://www.coolermaster.com/en-global/products/masterfan-mf120-halo2-white-edition     

   Author: Martin Looker (Silicon Labs)
           Tamas Jozsi (Silicon Labs)           
 */

// MATTER DEFINES

#define MATTER_ENABLED 1

#define MATTER_STATES 6
#define MATTER_STATE_DISABLED 0
#define MATTER_STATE_LEAVE 1
#define MATTER_STATE_COMMISSION 2
#define MATTER_STATE_CONNECT 3
#define MATTER_STATE_DISCOVER 4
#define MATTER_STATE_ONLINE 5

// MATTER INCLUDES

#if MATTER_ENABLED
#include <Matter.h>
#include <MatterFan.h>
#endif

// MATTER GLOBAL VARIABLES

uint8_t matterState = MATTER_STATE_DISABLED;
char matterStates[MATTER_STATES + 1][11] = { "Disabled", "Leave", "Commission", "Connect", "Discover", "Online", "Unknown" };
#if MATTER_ENABLED
MatterFan matter_fan;
#endif

// ANALOG DEFINES

#define ANALOG_WRITE_BITS 10
#define ANALOG_WRITE_MAX 0b1111111111
#define ANALOG_WRITE_FAN_MAX ANALOG_WRITE_MAX
#define ANALOG_WRITE_FAN_MIN 160
#define ANALOG_WRITE_MIN 0

// FAN DEFINES

#define PIN_FAN_CONTROL D7  // Noctua=Blue
#define PIN_FAN_TACHO D6    // Noctua=Green
#define FAN_MODES 7
#define FAN_MODE_OFF 0
#define FAN_MODE_LOW 1
#define FAN_MODE_MED 2
#define FAN_MODE_HIGH 3
#define FAN_MODE_ON 4
#define FAN_MODE_AUTO 5
#define FAN_MODE_SMART 6

// FAN GLOBAL VARIABLES

uint32_t fanTachoCounter = 0;
PinStatus fanTachoInterrupt = RISING;  // CHANGE, RISING, FALLING
uint32_t fanTachoCounterPerRev = 2;    // 4 for CHANGE, 2 for RISING and FALLING
uint32_t fanRpmTachoCount = 0;
uint32_t fanRpmMillis;
uint32_t fanRpmPeriod = 3000;
uint32_t fanRpmMultiplier = 60000 / fanRpmPeriod;  // For calculating RPM
uint32_t fanRpm;
char fanModeStrings[FAN_MODES + 1][8] = { "Off", "Low", "Med", "High", "On", "Auto", "Smart", "Unknown" };
uint8_t fanModePercents[] = { 0xFF, 0, 50, 100, 0xFF, 75, 25 };
uint8_t fanMode = FAN_MODE_OFF;
uint8_t fanPercent = 0;
bool fanIsOn = false;

// BUTTON DEFINES

#define PIN_BUTTON BTN_BUILTIN
#define PIN_BUTTON_DOWN LOW
#define PIN_BUTTON_UP HIGH

// GLOBAL BUTTON VARIABLES

bool buttonIsDown = false;
uint8_t buttonDebounce = 0;
uint8_t buttonFanModes[] = {
  FAN_MODE_OFF,
  FAN_MODE_LOW,
  FAN_MODE_SMART,
  FAN_MODE_MED,
  FAN_MODE_AUTO,
  FAN_MODE_HIGH
};

// COLOR GLOBAL VARIABLES

bool colorRedIsOn = true;
bool colorGreenIsOn = false;
bool colorBlueIsOn = false;
uint8_t colorRedLevel = 0xFF;
uint8_t colorGreenLevel = 0;
uint8_t colorBlueLevel = 0;

// ARGB INCLUDES

#include <ezWS2812.h>

// ARGB DEFINES

#define PIN_ARGB D11
#define ARGB_INNER_MAX 12
#define ARGB_OUTER_MAX 20
#define ARGB_MAX ARGB_INNER_MAX + ARGB_OUTER_MAX

#define ARGB_CONFIG_ARCTIC_P12 0          // Inner = 12, Outer = 0
#define ARGB_CONFIG_COOLERMASTER_HALO2 1  // Inner = 8, Outer = 16

#define ARGB_1S_LOOP_RPM 1440  // 720, 1440, 2160 produce integer tacho counts for 8, 12, 16 LEDs
#define ARGB_FADE 1            // Uses a fade on the ARGB LEDs

// ARGB GLOBAL VARIABLES

uint8_t argbInnerConfig[7] = { 12, 8, 0, 0, 0, 0, 0 };
uint8_t argbOuterConfig[7] = { 0, 16, 0, 0, 0, 0, 0 };
uint8_t argbInnerFadeForward[ARGB_INNER_MAX];
uint8_t argbOuterFadeForward[ARGB_OUTER_MAX];
uint8_t argbInnerFadeReverse[ARGB_INNER_MAX];
uint8_t argbOuterFadeReverse[ARGB_OUTER_MAX];
uint8_t argbRed[ARGB_MAX];
uint8_t argbGreen[ARGB_MAX];
uint8_t argbBlue[ARGB_MAX];
uint8_t argbInnerCount = 0;
uint8_t argbOuterCount = 0;
uint8_t argbCount = 0;
uint8_t argbInnerIndex = 0;
uint8_t argbOuterIndex = 0;
uint32_t argbTachoCount = 0;
uint32_t argbTachoPerStep;
ezWS2812 argb(ARGB_MAX);

// RGB LED DEFINES

#define PIN_RGB_RED LEDR
#define PIN_RGB_GREEN LEDG
#define PIN_RGB_BLUE LEDB
#define PIN_RGB_ON LOW
#define PIN_RGB_OFF HIGH

// RGB LED GLOBAL VARIABLES

bool rgbIsOn = true;
uint32_t rgbTachoCount = 0;
uint32_t rgbTachoPerStep;

// OLED INCLUDES

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include "qrcode.h"

// OLED DEFINES

#define PIN_OLED_SDA A4
#define PIN_OLED_SCL A5
#define OLED_WIDTH 128
#define OLED_HEIGHT 128

// OLED GLOBAL VARIABLES

bool oledEnabled = false;
Adafruit_SH1107 oled = Adafruit_SH1107(OLED_WIDTH, OLED_HEIGHT, &Wire, -1, 1000000, 100000);

// TIMER GLOBAL VARIABLES

uint32_t nowMillis;

// ARDUINO FUNCTIONS

void setup() {
  Serial.begin(115200);
#if MATTER_ENABLED
  Serial.println("MATTER ARGB FAN");
#else
  Serial.println("ARGB FAN");
#endif
  fanSetup();
  buttonSetup();
  colorSet(true, false, false);  // Red
  argbSetup(0, 0);
  rgbSetup();
  oledSetup();
#if MATTER_ENABLED
  matterSetup();
#else
  oledWriteText();
#endif
  nowMillis = fanRpmMillis = millis();
  fanRpmTachoCount = argbTachoCount = rgbTachoCount = fanTachoCounter;
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACHO), fanTachoIsr, fanTachoInterrupt);
}

void loop() {
  bool oledUpdate = false;
  nowMillis = millis();
  argbLoop(false, 1);
  rgbLoop();
  if (buttonLoop()) oledUpdate = true;
  argbLoop(false, 1);
  rgbLoop();
#if MATTER_ENABLED
  if (matterLoopMode()) oledUpdate = true;
  argbLoop(false, 1);
  rgbLoop();
  if (matterLoopOnOffPercent()) oledUpdate = true;
  argbLoop(false, 1);
  rgbLoop();
#endif
  if (fanLoop()) oledUpdate = true;
  argbLoop(false, 1);
  rgbLoop();
  if (oledUpdate) oledWriteText();
  argbLoop(false, 1);
  rgbLoop();
}

// MATTER FUNCTIONS

#if MATTER_ENABLED
void matterSetup() {
  uint32_t delayMs = 100;

  Serial.println("matterSetup()");
  Matter.begin();
  matter_fan.begin();

  // Matter decommission
  bool decommission = false;
  bool buttonState = digitalRead(PIN_BUTTON);
  if (buttonState == PIN_BUTTON_DOWN /*&& Matter.isDeviceCommissioned()*/) {
    matterState = MATTER_STATE_LEAVE;
    oledWriteText();
    Serial.println("Decommission button pressed, release to continue...");
    decommission = true;
    if (argbOuterCount > 0) delayMs = (2000 / argbOuterCount);
    else if (argbInnerCount > 0) delayMs = (2000 / argbInnerCount);
    else delayMs = 200;
    while (buttonState == PIN_BUTTON_DOWN) {
      buttonState = digitalRead(PIN_BUTTON);
#if ARGB_FADE
      argbStepFade(true, -1);
#else
      argbStepSingle(true);
#endif
      rgbToggle();
      delay(delayMs);
    }
    if (decommission) {
      Serial.println("Decommissioning");
      Matter.decommission();
    }
  }

  // Matter commission
  if (!Matter.isDeviceCommissioned()) {
    matterState = MATTER_STATE_COMMISSION;
    oledWriteQRCode();
    Serial.println("Matter device is not commissioned");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
    if (argbOuterCount > 0) delayMs = (1500 / argbOuterCount);
    else if (argbInnerCount > 0) delayMs = (1500 / argbInnerCount);
    else delayMs = 150;
    Serial.println("Waiting for Matter commissioning...");
    while (!Matter.isDeviceCommissioned()) {
#if ARGB_FADE
      argbStepFade(true, -1);
#else
      argbStepSingle(true);
#endif
      rgbToggle();
      delay(delayMs);
    }
    Serial.println("Commissioned for Matter");
  }

  // Thread connect
  if (!Matter.isDeviceThreadConnected()) {
    matterState = MATTER_STATE_CONNECT;
    oledWriteText();
    Serial.println("Connecting to Thread network...");
    if (argbOuterCount > 0) delayMs = (1000 / argbOuterCount);
    else if (argbInnerCount > 0) delayMs = (1000 / argbInnerCount);
    else delayMs = 100;
    while (!Matter.isDeviceThreadConnected()) {
#if ARGB_FADE
      argbStepFade(true, -1);
#else
      argbStepSingle(true);
#endif
      rgbToggle();
      delay(delayMs);
    }
    Serial.println("Connected to Thread network");
  }

  // Matter device discovery
  if (!matter_fan.is_online()) {
    matterState = MATTER_STATE_DISCOVER;
    oledWriteText();
    Serial.println("Waiting for Matter device discovery...");
    if (argbOuterCount > 0) delayMs = (500 / argbOuterCount);
    else if (argbInnerCount > 0) delayMs = (500 / argbInnerCount);
    else delayMs = 50;
    while (!matter_fan.is_online()) {
#if ARGB_FADE
      argbStepFade(true, -1);
#else
      argbStepSingle(true);
#endif
      rgbToggle();
      delay(delayMs);
    }
  }

  if (matter_fan.is_online()) {
    matterState = MATTER_STATE_ONLINE;
    oledWriteText();
    Serial.println("Matter device is now discovered");
    colorSet(false, true, false);
#if ARGB_FADE
    argbStepFade(false, -1);
#else
    argbStepSingle(true);
#endif
    rgbOn();
  }
}
#endif

#if MATTER_ENABLED
bool matterLoopMode() {
  bool update = false;

  // Mode changed remotely ?
  uint8_t matterFanMode = (uint8_t)matter_fan.get_mode();
  if (fanMode != matterFanMode) {
    Serial.println("matterLoopMode()");
    Serial.print("  matter_fan.get_mode() = ");
    Serial.println(matterFanMode);
    // Apply data model mode to hardware
    fanModeSet(matterFanMode);
    update = true;
  }

  return update;
}
#endif

#if MATTER_ENABLED
bool matterLoopOnOffPercent() {
  bool update = false;

  // ON/Off or speed changed remotely ?
  bool matterFanOnOff = matter_fan.get_onoff();
  uint8_t matterFanPercent = matter_fan.get_percent();
  if (fanIsOn != matterFanOnOff || fanPercent != matterFanPercent) {
    Serial.println("matterLoopOnOffPercent()");
  }
  if (fanIsOn != matterFanOnOff) {
    Serial.print("  matter_fan.get_onoff() = ");
    Serial.println(matterFanOnOff);
  }
  if (fanPercent != matterFanPercent) {
    Serial.print("  matter_fan.get_percent() = ");
    Serial.println(matterFanPercent);
  }
  if (fanIsOn != matterFanOnOff || fanPercent != matterFanPercent) {
    // Apply data model mode to hardware
    fanSet(matterFanOnOff, matterFanPercent);
    update = true;
  }

  return update;
}
#endif

// FAN FUNCTIONS

void fanSetup() {
  analogWriteResolution(ANALOG_WRITE_BITS);
  pinMode(PIN_FAN_CONTROL, OUTPUT);
  analogWrite(PIN_FAN_CONTROL, ANALOG_WRITE_MIN);
  digitalWrite(PIN_FAN_CONTROL, LOW);
  pinMode(PIN_FAN_TACHO, INPUT_PULLUP);
}

bool fanLoop() {
  bool result = false;
  uint32_t tacho;
  uint32_t tachoCount;

  // Tacho timer fired ?
  if (nowMillis - fanRpmMillis >= fanRpmPeriod) {
    Serial.println("fanLoop()");
    // Restart timer
    fanRpmMillis = nowMillis;
    // Take local copy of rpm counter
    tacho = fanTachoCounter;
    tachoCount = tacho - fanRpmTachoCount;
    fanRpmTachoCount = tacho;
    // Calculate RPM
    fanRpm = (tachoCount * fanRpmMultiplier) / fanTachoCounterPerRev;
    // Update leds
    if (fanRpm == 0) {
      rgbOn();
    }
    Serial.print("  fanRPM = ");
    Serial.println(fanRpm);
    result = true;
  }

  return result;
}

void fanTachoIsr() {
  fanTachoCounter++;
}

void fanSet(bool on, uint8_t percent) {
  Serial.print("fanSet(");
  Serial.print(on);
  Serial.print(", ");
  Serial.print(percent);
  Serial.println(")");
  // Update fan data
  fanIsOn = on;
  fanPercent = percent;
  if (fanIsOn) {
    // Look for fan mode matching percent and apply if different
    bool fanModeFound = false;
    for (uint8_t i = 0; i < FAN_MODES && fanModeFound == false; i++) {
      if (fanPercent == fanModePercents[i]) {
        fanModeFound = true;
        if (fanMode != i) {
          fanMode = i;
          Serial.print("  fanMode = ");
          Serial.println(fanMode);
        }
      }
    }
    // No matching fan mode percent so set to on if different
    if (fanModeFound == false) {
      if (fanMode != FAN_MODE_ON) {
        fanMode = FAN_MODE_ON;
        Serial.print("  fanMode = ");
        Serial.println(fanMode);
      }
    }
  } else {
    if (fanMode != FAN_MODE_OFF) {
      fanMode = FAN_MODE_OFF;
      Serial.print("  fanMode = ");
      Serial.println(fanMode);
    }
  }
  // Update matter data
#if MATTER_ENABLED
  if (matter_fan.get_onoff() != fanIsOn) {
    matter_fan.set_onoff(fanIsOn);
    Serial.print("  matter_fan.set_onoff(");
    Serial.print(fanIsOn);
    Serial.println(")");
  }
  if (matter_fan.get_percent() != fanPercent) {
    matter_fan.set_percent(fanPercent);
    Serial.print("  matter_fan.set_percent(");
    Serial.print(fanPercent);
    Serial.println(")");
  }
  if (matter_fan.get_mode() != (DeviceFan::fan_mode_t)fanMode) {
    matter_fan.set_mode((DeviceFan::fan_mode_t)fanMode);
    Serial.print("  matter_fan.set_mode(");
    Serial.print(fanMode);
    Serial.println(")");
  }
#endif
  // Update colors and drive fan
  if (fanIsOn) {
    colorSet(false, false, true);
    if (fanPercent >= 100) {
      analogWrite(PIN_FAN_CONTROL, ANALOG_WRITE_MIN);
      digitalWrite(PIN_FAN_CONTROL, HIGH);
    } else {
      digitalWrite(PIN_FAN_CONTROL, LOW);
      analogWrite(PIN_FAN_CONTROL, map(fanPercent, 0, 100, ANALOG_WRITE_FAN_MIN, ANALOG_WRITE_FAN_MAX));
    }
  } else {
    colorSet(false, true, false);
    analogWrite(PIN_FAN_CONTROL, ANALOG_WRITE_MIN);
    digitalWrite(PIN_FAN_CONTROL, LOW);
  }
}

void fanModeSet(uint8_t mode) {
  Serial.print("fanModeSet(");
  Serial.print(mode);
  Serial.println(")");
  fanMode = mode;
#if MATTER_ENABLED
  if (matter_fan.get_mode() != (DeviceFan::fan_mode_t)mode) {
    matter_fan.set_mode((DeviceFan::fan_mode_t)mode);
    Serial.print("  matter_fan.set_mode(");
    Serial.print(fanMode);
    Serial.println(")");
  }
#endif
  if (fanMode == FAN_MODE_OFF) {
    fanSet(false, fanPercent);
  } else if (fanMode == FAN_MODE_ON) {
    fanSet(true, fanPercent);
  } else if (fanMode < FAN_MODES) {
    fanSet(true, fanModePercents[fanMode]);
  }
}

// BUTTON FUNCTIONS

void buttonSetup() {
#if (PIN_BUTTON_DOWN == LOW)
  pinMode(PIN_BUTTON, INPUT_PULLUP);
#else
  pinMode(PIN_BUTTON, INPUT);
#endif
}

bool buttonLoop() {
  bool update = false;
  bool buttonRead = digitalRead(PIN_BUTTON);
  buttonDebounce <<= 1;
  if (buttonRead == PIN_BUTTON_DOWN) buttonDebounce |= 1;
  if (buttonIsDown) {
    if (buttonDebounce == 0) {
      buttonIsDown = false;
      Serial.println("buttonLoop()");
      Serial.print("  buttonIsDown = ");
      Serial.println(buttonIsDown);
    }
  } else {
    if (buttonDebounce == 0xFF) {
      buttonIsDown = true;
      Serial.println("buttonLoop()");
      Serial.print("  buttonIsDown = ");
      Serial.println(buttonIsDown);
      // Start with invalid index
      uint8_t buttonFanModeIndex = sizeof(buttonFanModes);
      // Look for current index
      for (uint8_t i = 0; i < sizeof(buttonFanModes); i++) {
        if (fanMode == buttonFanModes[i]) {
          buttonFanModeIndex = i;
        }
      }
      // Go to next button fan mode
      buttonFanModeIndex++;
      if (buttonFanModeIndex >= sizeof(buttonFanModes)) buttonFanModeIndex = 0;
      fanModeSet(buttonFanModes[buttonFanModeIndex]);
      update = true;
    }
  }

  return update;
}

// COLOR FUNCTIONS

void colorSet(bool red, bool green, bool blue) {
  if (red != colorRedIsOn || green != colorGreenIsOn || blue != colorBlueIsOn) {
    colorRedIsOn = red;
    colorGreenIsOn = green;
    colorBlueIsOn = blue;
    colorRedLevel = (colorRedIsOn ? 0xFF : 0);
    colorGreenLevel = (colorGreenIsOn ? 0xFF : 0);
    colorBlueLevel = (colorBlueIsOn ? 0xFF : 0);
  }
}

// ARGB FUNCTIONS

void argbSetup(uint8_t setupInner, uint8_t setupOuter) {
  uint8_t i;
  uint8_t val;
  bool jumper;

  Serial.print("argbSetup(");
  Serial.print(setupInner);
  Serial.print(", ");
  Serial.print(setupOuter);
  Serial.println(")");

  // Hardcoded ARGB configuration ?
  if (setupInner > 0 || setupOuter > 0) {
    argbInnerCount = setupInner;
    argbOuterCount = setupOuter;
  }
  // Jumper ARGB configuration
  else {
    if (jumperConnected(A0, A1)) {
      argbOuterCount = argbOuterConfig[ARGB_CONFIG_ARCTIC_P12];
      argbInnerCount = argbInnerConfig[ARGB_CONFIG_ARCTIC_P12];
    } else if (jumperConnected(A1, A2)) {
      argbOuterCount = argbOuterConfig[ARGB_CONFIG_COOLERMASTER_HALO2];
      argbInnerCount = argbInnerConfig[ARGB_CONFIG_COOLERMASTER_HALO2];
    } else if (jumperConnected(A3, A4)) {
      argbOuterCount = argbOuterConfig[2];
      argbInnerCount = argbInnerConfig[2];
    } else if (jumperConnected(A4, D6)) {
      argbOuterCount = argbOuterConfig[3];
      argbInnerCount = argbInnerConfig[3];
    } else if (jumperConnected(D6, D5)) {
      argbOuterCount = argbOuterConfig[4];
      argbInnerCount = argbInnerConfig[4];
    } else if (jumperConnected(D5, D4)) {
      argbOuterCount = argbOuterConfig[5];
      argbInnerCount = argbInnerConfig[5];
    } else if (jumperConnected(D4, D3)) {
      argbOuterCount = argbOuterConfig[6];
      argbInnerCount = argbInnerConfig[6];
    } else if (jumperConnected(D3, D2)) {
      argbOuterCount = argbOuterConfig[7];
      argbInnerCount = argbInnerConfig[7];
    } else {
      argbOuterCount = 0;
      argbInnerCount = 0;
    }
  }
  Serial.print("  argbOuterCount = ");
  Serial.println(argbOuterCount);
  Serial.print("  argbInnerCount = ");
  Serial.println(argbInnerCount);

  // Rationalise counts
  if (argbInnerCount > ARGB_INNER_MAX) argbInnerCount = ARGB_INNER_MAX;
  if (argbOuterCount > ARGB_OUTER_MAX) argbOuterCount = ARGB_OUTER_MAX;
  argbCount = argbInnerCount + argbOuterCount;

  if (argbCount > 0) {

    if (argbInnerCount > 0) {
      // Initialise inner forward fade value array
      val = 100;
      for (i = 0; i < argbInnerCount; i++) {
        argbInnerFadeForward[i] = val;
        val = val / 4;
        if (val < 6) val = 0;
      }
      // Initialise inner reverse fade value array
      for (i = 0; i < argbInnerCount; i++) {
        argbInnerFadeReverse[argbInnerCount - 1 - i] = argbInnerFadeForward[i];
      }
    }

    if (argbOuterCount > 0) {
      // Initialise outer forward fade value array
      val = 100;
      for (i = 0; i < argbOuterCount; i++) {
        argbOuterFadeForward[i] = val;
        val = val / 2;
        if (val < 6) val = 0;
      }
      // Initialise outer reverse fade value array
      for (i = 0; i < argbOuterCount; i++) {
        argbOuterFadeReverse[argbOuterCount - 1 - i] = argbOuterFadeForward[i];
      }
    }

    // Calculate tacho count to achieve 1 revolution of the ARGBs over 1 second
    // at defined RPM (or as close as possible)
    uint32_t tachoPerSecond = (ARGB_1S_LOOP_RPM * fanTachoCounterPerRev) / 60;
    if (argbOuterCount > 0) {
      argbTachoPerStep = tachoPerSecond / argbOuterCount;
    } else if (argbInnerCount > 0) {
      argbTachoPerStep = tachoPerSecond / argbInnerCount;
    }
    if (argbTachoPerStep == 0) argbTachoPerStep = 1;
    Serial.print("  argbTachoPerStep = ");
    Serial.println(argbTachoPerStep);

    // Start allowing the LEDs to be used
    argb.begin();
  }
}

void argbLoop(bool reverse, int8_t step) {

  if (argbCount > 0) {
    // Take local copy of the argb counter
    uint32_t tacho = fanTachoCounter;
    uint32_t tachoCount = tacho - argbTachoCount;
    if (tachoCount >= argbTachoPerStep) {
#if ARGB_FADE
      argbStepFade(reverse, step);
#else
      argbStepSingle(step);
#endif
      if (tachoCount > argbTachoPerStep) Serial.print("!");
      argbTachoCount = tacho;
    }
  }
}

void argbStepSingle(int8_t step) {

  if (argbCount > 0) {
    if (argbOuterCount > 0) {
      if (step < 0) {
        argbOuterIndex--;
        if (argbOuterIndex >= argbOuterCount) argbOuterIndex = argbOuterCount - 1;
      } else if (step > 0) {
        argbOuterIndex++;
        if (argbOuterIndex >= argbOuterCount) argbOuterIndex = 0;
      }
      argbInnerIndex = argbOuterIndex / 2;
    } else if (argbInnerCount > 0) {
      if (step < 0) {
        argbInnerIndex--;
        if (argbInnerIndex >= argbInnerCount) argbInnerIndex = argbInnerCount - 1;
      } else if (step > 0) {
        argbInnerIndex++;
        if (argbInnerIndex >= argbInnerCount) argbInnerIndex = 0;
      }
    }

    noInterrupts();
    for (uint8_t i = 0; i < argbInnerCount; i++) {
      if (i == argbInnerIndex) {
        argb.set_pixel(
          1,                // Number of argb
          colorRedLevel,    // Red level
          colorGreenLevel,  // Green level
          colorBlueLevel,   // Blue level
          100,              // Brightness
          false);           // End of transfer
      } else {
        argb.set_pixel(
          1,    // Number of argb
          0,    // Red level
          0,    // Green level
          0,    // Blue level
          100,  // Brightness
          false);
      }  // End of transfer
    }
    for (uint8_t i = 0; i < argbOuterCount; i++) {
      if (i == argbOuterIndex) {
        argb.set_pixel(
          1,                // Number of argb
          colorRedLevel,    // Red level
          colorGreenLevel,  // Green level
          colorBlueLevel,   // Blue level
          100,              // Brightness
          false);           // End of transfer
      } else {
        argb.set_pixel(
          1,    // Number of argb
          0,    // Red level
          0,    // Green level
          0,    // Blue level
          100,  // Brightness
          false);
      }
    }
    argb.end_transfer();
    interrupts();
  }
}

void argbStepFade(bool reverse, int8_t step) {

  if (argbCount > 0) {
    if (argbOuterCount > 0) {
      if (step < 0) {
        argbOuterIndex--;
        if (argbOuterIndex >= argbOuterCount) argbOuterIndex = argbOuterCount - 1;
      } else if (step > 0) {
        argbOuterIndex++;
        if (argbOuterIndex >= argbOuterCount) argbOuterIndex = 0;
      }
      argbInnerIndex = argbOuterIndex / 2;
    } else if (argbInnerCount > 0) {
      if (step < 0) {
        argbInnerIndex--;
        if (argbInnerIndex >= argbInnerCount) argbInnerIndex = argbInnerCount - 1;
      } else if (step > 0) {
        argbInnerIndex++;
        if (argbInnerIndex >= argbInnerCount) argbInnerIndex = 0;
      }
    }

    noInterrupts();
    if (reverse) {
      for (uint8_t i = 0, p = argbInnerIndex; i < argbInnerCount; i++, p++) {
        if (p >= argbInnerCount) p = 0;
        argb.set_pixel(
          1,                        // Number of argb
          colorRedLevel,            // Red level
          colorGreenLevel,          // Green level
          colorBlueLevel,           // Blue level
          argbInnerFadeReverse[p],  // Brightness
          false);                   // End of transfer
      }
      for (uint8_t i = 0, p = argbOuterIndex; i < argbOuterCount; i++, p++) {
        if (p >= argbOuterCount) p = 0;
        argb.set_pixel(
          1,                        // Number of argb
          colorRedLevel,            // Red level
          colorGreenLevel,          // Green level
          colorBlueLevel,           // Blue level
          argbOuterFadeReverse[p],  // Brightness
          false);                   // End of transfer
      }
    } else {
      for (uint8_t i = 0, p = argbInnerIndex; i < argbInnerCount; i++, p++) {
        if (p >= argbInnerCount) p = 0;
        argb.set_pixel(
          1,                        // Number of argb
          colorRedLevel,            // Red level
          colorGreenLevel,          // Green level
          colorBlueLevel,           // Blue level
          argbInnerFadeForward[p],  // Brightness
          false);                   // End of transfer
      }
      for (uint8_t i = 0, p = argbOuterIndex; i < argbOuterCount; i++, p++) {
        if (p >= argbOuterCount) p = 0;
        argb.set_pixel(
          1,                        // Number of argb
          colorRedLevel,            // Red level
          colorGreenLevel,          // Green level
          colorBlueLevel,           // Blue level
          argbOuterFadeForward[p],  // Brightness
          false);                   // End of transfer
      }
    }
    argb.end_transfer();
    interrupts();
  }
}

// RGB FUNCTIONS

void rgbSetup() {
  // Initialise RGB
  pinMode(PIN_RGB_RED, OUTPUT);
  pinMode(PIN_RGB_GREEN, OUTPUT);
  pinMode(PIN_RGB_BLUE, OUTPUT);
  rgbOff();
  rgbTachoPerStep = (argbTachoPerStep * 4);
}

void rgbLoop() {
  // Take local copy of the argb counter
  uint32_t tacho = fanTachoCounter;
  uint32_t tachoCount = tacho - rgbTachoCount;
  if (tachoCount >= rgbTachoPerStep) {
    rgbToggle();
    rgbTachoCount = tacho;
  }
}

void rgbOn() {
  // Set RGB
  digitalWrite(PIN_RGB_RED, (colorRedIsOn ? PIN_RGB_ON : PIN_RGB_OFF));
  digitalWrite(PIN_RGB_GREEN, (colorGreenIsOn ? PIN_RGB_ON : PIN_RGB_OFF));
  digitalWrite(PIN_RGB_BLUE, (colorBlueIsOn ? PIN_RGB_ON : PIN_RGB_OFF));
  // RGB is on
  rgbIsOn = true;
}

void rgbOff() {
  // Set RGB
  digitalWrite(PIN_RGB_RED, PIN_RGB_OFF);
  digitalWrite(PIN_RGB_GREEN, PIN_RGB_OFF);
  digitalWrite(PIN_RGB_BLUE, PIN_RGB_OFF);
  // RGB is off
  rgbIsOn = false;
}

void rgbToggle() {
  if (rgbIsOn) rgbOff();
  else rgbOn();
}

// OLED FUNCTIONS

bool oledSetup() {
  uint8_t antiFlicker[2] = { SH110X_SETDISPLAYCLOCKDIV, 0xF0 };

  Serial.println("oledSetup()");
  Wire.begin();
  delay(250);
  oledEnabled = oled.begin(0x3D, true);
  oled.oled_commandList(antiFlicker, sizeof(antiFlicker));
  Serial.print("  oledEnabled =  ");
  Serial.println(oledEnabled);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.display();

  return oledEnabled;
}

bool oledWriteText() {
  if (oledEnabled) {
    int16_t yTextTop = -3;
    char line[20];

    // Set up
    oled.clearDisplay();
    // Inverted (black on white) display
    //oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SH110X_WHITE);
    //oled.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SH110X_WHITE);
    //oled.setTextColor(SH110X_BLACK);

    // Title
    oled.setFont(&FreeSansBold12pt7b);
#if MATTER_ENABLED
    yTextTop = oledCenterText(yTextTop, "MATTER");
    yTextTop = oledCenterText(yTextTop, "ARGB FAN") + 4;
#else
    yTextTop = oledCenterText(yTextTop, "ARGB");
    yTextTop = oledCenterText(yTextTop, "FAN") + 4;
#endif

    // Matter state
    oled.setFont(&FreeSans12pt7b);
    if (matterState < MATTER_STATES) {
      yTextTop = oledCenterText(yTextTop, matterStates[matterState]) + 4;
    } else {
      yTextTop = oledCenterText(yTextTop, matterStates[MATTER_STATES]) + 4;
    }

    // Mode and percent
    sprintf(line, "%s %d", fanModeStrings[fanMode], fanPercent);
    yTextTop = oledCenterText(yTextTop, line) + 4;

    // RPM
    sprintf(line, "%d RPM", fanRpm);
    yTextTop = oledCenterText(yTextTop, line) + 4;

    // Update display
    noInterrupts();
    oled.display();
    interrupts();
    oled.setContrast(255);
  }

  return oledEnabled;
}

#if MATTER_ENABLED
bool oledWriteQRCode() {
  if (oledEnabled) {
    Serial.print("oledWriteQRCode = ");
    oled.clearDisplay();

    // Create the QR code
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    String payloadString = Matter.getOnboardingQRCodePayload();
    char payload[30];

    payloadString.toCharArray(payload, sizeof(payload));
    qrcode_initText(&qrcode, qrcodeData, 4, ECC_LOW, payload);
    Serial.print("  qrcode.size = ");
    Serial.println(qrcode.size);

    uint8_t maxqrcode = OLED_HEIGHT;
    if (OLED_WIDTH < maxqrcode) maxqrcode = OLED_WIDTH;
    Serial.print("  maxqrcode = ");
    Serial.println(maxqrcode);
    uint8_t modqrcode = maxqrcode / qrcode.size;
    Serial.print("  modqrcode = ");
    Serial.println(modqrcode);
    uint8_t xOffset = (OLED_WIDTH - (qrcode.size * modqrcode)) / 2;
    uint8_t yOffset = (OLED_HEIGHT - (qrcode.size * modqrcode)) / 2;

    oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SH110X_WHITE);
    oled.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SH110X_WHITE);
    for (uint8_t yQR = 0, yOLED = yOffset; yQR < qrcode.size; yQR++, yOLED += modqrcode) {
      // Each horizontal module
      for (uint8_t xQR = 0, xOLED = xOffset; xQR < qrcode.size; xQR++, xOLED += modqrcode) {
        if (qrcode_getModule(&qrcode, xQR, yQR)) {
          if (modqrcode < 2) oled.drawPixel(xOLED, yOLED, SH110X_BLACK);
          else {
            oled.drawRect(xOLED, yOLED, modqrcode, modqrcode, SH110X_BLACK);
            oled.fillRect(xOLED, yOLED, modqrcode, modqrcode, SH110X_BLACK);
          }
        }
      }
    }

    noInterrupts();
    oled.display();
    interrupts();
    oled.setContrast(0);  // Dim for easier scanning
  }

  return oledEnabled;
}
#endif

int16_t oledCenterText(int16_t yTop, char *text) {
  int16_t xA, yA;
  int16_t xB, yB;
  uint16_t wB, hB;

  oled.getTextBounds(text, 0, OLED_HEIGHT / 2, &xB, &yB, &wB, &hB);
  xA = (OLED_WIDTH - wB) / 2;
  oled.getTextBounds("Jj", 0, OLED_HEIGHT / 2, &xB, &yB, &wB, &hB);
  yA = yTop + hB;
  oled.setCursor(xA, yA);
  oled.print(text);

  return yA;
}

bool jumperConnected(int pinA, int pinB) {
  bool result = false;

  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, OUTPUT);
  digitalWrite(pinB, HIGH);
  if (digitalRead(pinA) == HIGH) {
    digitalWrite(pinB, LOW);
    if (digitalRead(pinA) == LOW) {
      digitalWrite(pinB, HIGH);
      if (digitalRead(pinA) == HIGH) {
        digitalWrite(pinB, LOW);
        if (digitalRead(pinA) == LOW) {
          result = true;
        }
      }
    }
  }
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);

  return result;
}