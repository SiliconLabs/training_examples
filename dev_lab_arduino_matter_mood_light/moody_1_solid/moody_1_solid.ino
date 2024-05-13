/******************************************************************************
 * Copyright 2024 Silicon Laboratories Inc. www.silabs.com
 ******************************************************************************
 * Arduino Matter Mood Light - Training Example
 *
 * Step 1 - Solid Colors
 * 
 * Sets up a Matter device with two color bulb endpoints each of which can be 
 * set independently
 *
 * Button 0 cycles through display modes: 
 * 0 - Solid color from bulb 0
 * 1 - Solid color from bulb 1 
 *
 * Compatible boards:
 * - Arduino Nano Matter
 * - SparkFun Thing Plus MGM240P Matter (an external button is required)
 * - xG24 Dev Kit
 * - xG24 Explorer Kit
 * 
 * Author: Martin Looker (Silicon Labs)
 * 
 ******************************************************************************* 
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided \'as-is\', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution. 
 * 
 *******************************************************************************
 * EXPERIMENTAL QUALITY
 * 
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 * 
 ******************************************************************************/

// Library includes
#include <Matter.h>
#include <MatterLightbulb.h>
#include <ezWS2812.h>

// Application includes
#include "fast_hsv2rgb.h"  // https://www.vagrearg.org/content/hsvrgb

// Defines
#define BULB_BOOST_SATURATION 0   // Boost saturation 0-255, in Google Home the edge of the color wheel is only 80% saturated a value of 51 (20%) here will boost to full saturation
#define BULB_COUNT 2              // Number of bulb endpoints
#define PIXEL_COUNT 80            // Number of WS2812 RGB LEDs, adjust as required
#define LED_MODE_PIN LED_BUILTIN  // Pin for mode LED
#define BTN_MODE_PIN BTN_BUILTIN  // Pin for mode button
#define BTN_ACTIVE LOW            // State of button when pressed
#define PIXEL_MODE_SOLID_0 0      // Solid color 0
#define PIXEL_MODE_SOLID_1 1      // Solid color 1
#define PIXEL_MODE_COUNT 2        // Number of modes

// General data
uint32_t now_millis;  // For timers

// Digital IO data
uint16_t dio_period = 100;  // Period to poll, update digital IOs
uint32_t dio_millis;        // Timer for digital IO
uint16_t led_bit = 0b1;     // Bit for LED flashing
bool btn_state;             // Button state

// Matter bulb data
MatterColorLightbulb bulbs[BULB_COUNT];         // Matter color bulb objects
bool bulb_states[BULB_COUNT] = { true, true };  // Bulb states off/on
uint16_t bulb_hues[BULB_COUNT] = { 240, 300 };  // Bulb hues, blue, magenta (0-359)
uint8_t bulb_sats[BULB_COUNT] = { 254, 254 };   // Bulb saturations (1-254)
uint8_t bulb_vals[BULB_COUNT] = { 254, 254 };   // Bulb values/brightness (1-254)
uint8_t bulb_reds[BULB_COUNT];                  // Bulb red values
uint8_t bulb_greens[BULB_COUNT];                // Bulb green values
uint8_t bulb_blues[BULB_COUNT];                 // Bulb blue values
uint32_t bulb_period = 100;                     // Bulb monitoring period
uint32_t bulb_millis;                           // Bulb monitoring timer

// Pixel data
ezWS2812 pixels(PIXEL_COUNT);                                      // RGB pixels object
uint8_t pixel_mode = PIXEL_MODE_SOLID_0;                           // Default display mode
uint16_t pixel_mode_led_masks[PIXEL_MODE_COUNT] = { 0b1, 0b101 };  // LED flash patterns for each mode
uint32_t pixel_period = 40;                                        // Pixel update period
uint32_t pixel_millis;                                             // Pixel update timer

// Function prototypes
bool read_bulbs();                                                                           // Read bulb data and react
void write_gradients();                                                                      // Calculate gradients from bulb data
void write_pixels();                                                                         // Write colors to pixels
void fast_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b);  // Integer HSV to RGB conversion

// Setup, called once at start up, put initialisation code here
void setup() {
  // Data
  bool led_on = true;
  // Initialise serial port
  Serial.begin(115200);
  Serial.printf("\nMATTER MOOD LIGHT (SOLID)");
  // Initialise LED to on
  pinMode(LED_MODE_PIN, OUTPUT);
  digitalWrite(LED_MODE_PIN, led_on);
  // Initialise buttons
  #if (BTN_ACTIVE==LOW)  
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  #else
  pinMode(BTN_MODE_PIN, INPUT);  
  #endif
  btn_state = digitalRead(BTN_MODE_PIN);
  // Initialise matter
  Matter.begin();
  for (uint8_t u = 0; u < BULB_COUNT; u++) {
    bulbs[u].begin();
    bulbs[u].boost_saturation(BULB_BOOST_SATURATION);
    bulbs[u].set_onoff(bulb_states[u]);
    bulbs[u].set_true_hue(bulb_hues[u]);
    bulbs[u].set_saturation(bulb_sats[u]);
    bulbs[u].set_brightness(bulb_vals[u]);
    fast_hsv_to_rgb(bulb_hues[u], bulb_sats[u], bulb_vals[u], &bulb_reds[u], &bulb_greens[u], &bulb_blues[u]);
    // Debug
    Serial.printf("\n%u: bulbs[%u]: state = %u, h = %u, s = %u, v = %u, r = %u, g = %u, b = %u",
                  millis(), u, bulb_states[u],
                  bulb_hues[u], bulb_sats[u], bulb_vals[u],
                  bulb_reds[u], bulb_greens[u], bulb_blues[u]);
  }
  // Initialise pixels to dim magenta
  pixels.begin();
  noInterrupts();
  pixels.set_all(0x20, 0, 0x20);
  interrupts();
  // Matter commissioning
  if (!Matter.isDeviceCommissioned()) {
    Serial.printf("\nMatter device is not commissioned");
    Serial.printf("\nCommission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("\nManual pairing code: %s", Matter.getManualPairingCode().c_str());
    Serial.printf("\nQR code URL: %s", Matter.getOnboardingQRCodeUrl().c_str());
  }
  while (!Matter.isDeviceCommissioned()) {
    delay(200);
    if (led_on) led_on = false;
    else led_on = true;
    digitalWrite(LED_MODE_PIN, led_on);
  }
  // Set pixels to dim yellow
  noInterrupts();
  pixels.set_all(0x20, 0x20, 0);
  interrupts();
  // Matter connection
  Serial.printf("\nWaiting for Thread network...");
  while (!Matter.isDeviceThreadConnected()) {
    delay(200);
    if (led_on) led_on = false;
    else led_on = true;
    digitalWrite(LED_MODE_PIN, led_on);    
  }
  // Matter online
  Serial.printf("\nConnected to Thread network");
  // Set pixels to dim cyan
  noInterrupts();
  pixels.set_all(0, 0x20, 0x20);
  interrupts();
  // Bulbs online
  Serial.printf("\nWaiting for Matter device discovery...");
  while (bulbs[0].is_online() == false || bulbs[1].is_online() == false) {
    delay(200);
    if (led_on) led_on = false;
    else led_on = true;
    digitalWrite(LED_MODE_PIN, led_on);     
  }
  Serial.printf("\nMatter device is now online");
  // Turn off LED
  led_on = false;
  digitalWrite(LED_MODE_PIN, led_on);
  // Start loop timers
  now_millis = dio_millis = bulb_millis = pixel_millis = millis();
}

// Loop, called repeatedly, put processing code here
void loop() {
  // Update time
  now_millis = millis();
  // Bulb timer fired?
  if (now_millis - bulb_millis >= bulb_period) {
    // Restart timer
    bulb_millis = now_millis;
    // Read bulb status
    read_bulbs();
  }
  // Digital IO timer fired ?
  if (now_millis - dio_millis >= dio_period) {
    // Restart timer
    dio_millis = now_millis;
    // Read button
    bool state = digitalRead(BTN_MODE_PIN);
    // Button changed ?
    if (btn_state != state) {
      btn_state = state;
      // Button pressed ?
      if (btn_state == BTN_ACTIVE) {
        // Change mode
        pixel_mode++;
        if (pixel_mode >= PIXEL_MODE_COUNT) pixel_mode = PIXEL_MODE_SOLID_0;
        Serial.printf("\n%u: pixel_mode = %u", now_millis, pixel_mode);
      }
    }
    // Make sure a led bit is set
    if (led_bit == 0) {
      led_bit = 0b1;
    }
    // Update LEDs, on with off periods indicating modes
    if (led_bit & pixel_mode_led_masks[pixel_mode]) digitalWrite(LED_MODE_PIN, LED_BUILTIN_ACTIVE);
    else digitalWrite(LED_MODE_PIN, LED_BUILTIN_INACTIVE);
    // Update bit for next time
    led_bit <<= 1;
  }
  // Pixels timer fired ?
  if (now_millis - pixel_millis >= pixel_period) {
    // Restart timer
    pixel_millis = now_millis;
    // Write to pixels
    write_pixels();
  }
}

bool read_bulbs() {
  // Data
  bool updates[BULB_COUNT] = { false, false };
  bool state;
  uint16_t hue;
  uint8_t sat;
  uint8_t val;
  // Loop through bulbs
  for (uint8_t u = 0; u < BULB_COUNT; u++) {
    // Get current data
    state = bulbs[u].get_onoff();
    hue = bulbs[u].get_true_hue();
    sat = bulbs[u].get_saturation();
    val = bulbs[u].get_brightness();
    // State changed ?
    if (bulb_states[u] != state) {
      bulb_states[u] = state;
      updates[u] = true;
    }
    // Hue changed ?
    if (bulb_hues[u] != hue) {
      bulb_hues[u] = hue;
      updates[u] = true;
    }
    // Sat changed ?
    if (bulb_sats[u] != sat) {
      bulb_sats[u] = sat;
      updates[u] = true;
    }
    // Val changed ?
    if (bulb_vals[u] != val) {
      bulb_vals[u] = val;
      updates[u] = true;
    }
    // Something changed ?
    if (updates[u]) {
      // Bulb is on ?
      if (bulb_states[u]) {
        // Convert color
        fast_hsv_to_rgb(bulb_hues[u], bulb_sats[u], bulb_vals[u], &bulb_reds[u], &bulb_greens[u], &bulb_blues[u]);
      }
      // Bulb is off ?
      else {
        // Use black
        bulb_reds[u] = bulb_greens[u] = bulb_blues[u] = 0;
      }
      // Debug
      Serial.printf("\n%u: read_bulbs(%u) = %u, state = %u, h = %u, s = %u, v = %u, r = %u, g = %u, b = %u",
                    millis(), u, updates[u], bulb_states[u],
                    bulb_hues[u], bulb_sats[u], bulb_vals[u],
                    bulb_reds[u], bulb_greens[u], bulb_blues[u]);
    }
  }
  return (updates[0] | updates[1]);
}

// Write to pixels
void write_pixels() {
  // Which mode:
  switch (pixel_mode) {
    // Solid bulb 0 color
    case PIXEL_MODE_SOLID_0:
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, bulb_reds[0], bulb_greens[0], bulb_blues[0], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      break;

    // Solid bulb 1 color ?
    case PIXEL_MODE_SOLID_1:
    default:
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, bulb_reds[1], bulb_greens[1], bulb_blues[1], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      break;
  }
}

// Fast HSV to RGB conversion
void fast_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
  // Limit hue to 0-359
  if (h > 359) h %= 360;
  // Scale hue to range used by fast hsv
  uint16_t fast_h = map(h, 0, 359, 0, HSV_HUE_STEPS);
  // Convert
  fast_hsv2rgb_32bit(fast_h, s, v, r, g, b);
}
