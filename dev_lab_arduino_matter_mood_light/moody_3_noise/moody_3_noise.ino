/******************************************************************************
 * Copyright 2024 Silicon Laboratories Inc. www.silabs.com
 ******************************************************************************
 * Arduino Matter Mood Light - Training Example
 *
 * Step 3 - Color Noise
 *
 * Sets up a Matter device with two color bulb endpoints each of which can be 
 * set independently
 *
 * Button 0 cycles through display modes: 
 * 0 - Solid color from bulb 0
 * 1 - Solid color from bulb 1
 * 2 - Animated color gradient, short path round color wheel
 * 3 - Animated color gradient, long path round color wheel
 * 4 - Animated color noise, slow, short path round color wheel
 * 5 - Animated color noise, slow, long path round color wheel 
 * 6 - Animated color noise, fast, short path round color wheel
 * 7 - Animated color noise, fast, long path round color wheel    
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
#include "noise.h"         // https://auburn.github.io/FastNoiseLite + https://notisrac.github.io/FileToCArray

// Defines
#define BULB_BOOST_SATURATION 0        // Boost saturation 0-255, in Google Home the edge of the color wheel is only 80% saturated a value of 51 (20%) here will boost to full saturation
#define BULB_COUNT 2                   // Number of bulb endpoints
#define PIXEL_COUNT 80                 // Number of WS2812 RGB LEDs, adjust as required
#define GRADIENT_COUNT 256             // Size of gradient
#define LED_MODE_PIN LED_BUILTIN       // Pin for mode LED
#define BTN_MODE_PIN BTN_BUILTIN       // Pin for mode button
#define BTN_ACTIVE LOW                 // State of button when pressed
#define PIXEL_MODE_SOLID_0 0           // Solid color 0
#define PIXEL_MODE_SOLID_1 1           // Solid color 1
#define PIXEL_MODE_GRADIENT_SHORT 2    // Gradient short path round the color wheel
#define PIXEL_MODE_GRADIENT_LONG 3     // Gradient long path round the color wheel
#define PIXEL_MODE_NOISE_SLOW_SHORT 4  // Noise short path round the color wheel
#define PIXEL_MODE_NOISE_SLOW_LONG 5   // Noise long path round the color wheel
#define PIXEL_MODE_NOISE_FAST_SHORT 6  // Noise short path round the color wheel
#define PIXEL_MODE_NOISE_FAST_LONG 7   // Noise long path round the color wheel
#define PIXEL_MODE_COUNT 8             // Number of modes

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

// Gradient data
uint8_t gradient_short_reds[GRADIENT_COUNT];    // Short gradient red values
uint8_t gradient_short_greens[GRADIENT_COUNT];  // Short gradient green values
uint8_t gradient_short_blues[GRADIENT_COUNT];   // Short gradient blue values
uint8_t gradient_long_reds[GRADIENT_COUNT];     // Long gradient red values
uint8_t gradient_long_greens[GRADIENT_COUNT];   // Long gradient green values
uint8_t gradient_long_blues[GRADIENT_COUNT];    // Long gradient blue values
uint16_t gradient_index = 0;                    // Gradient index for animation
bool gradient_sub = false;                      // Gradient animation index direction

// Noise data
uint32_t noise_row = 0;      // Noise row for animation
uint32_t noise_col = 0;      // Noise column for animation
bool noise_row_sub = false;  // Noise row animation direction
bool noise_col_sub = false;  // Noise column animation direction

// Pixel data
ezWS2812 pixels(PIXEL_COUNT);                                                                                                                                          // RGB pixels object
uint8_t pixel_mode = PIXEL_MODE_NOISE_SLOW_SHORT;                                                                                                                      // Default display mode
uint16_t pixel_mode_led_masks[PIXEL_MODE_COUNT] = { 0b1, 0b101, 0b10101, 0b1010101, 0b1111111111111110, 0b1111111111111010, 0b1111111111101010, 0b1111111110101010 };  // LED flash patterns for each mode
uint32_t pixel_period = 40;                                                                                                                                            // Pixel update period
uint32_t pixel_millis;                                                                                                                                                 // Pixel update timer

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
  Serial.printf("\nMATTER MOOD LIGHT (NOISE)");
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
  // Initialise gradients
  write_gradients();
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
  // Data
  bool update = false;
  // Update time
  now_millis = millis();
  // Bulb timer fired?
  if (now_millis - bulb_millis >= bulb_period) {
    // Restart timer
    bulb_millis = now_millis;
    // Read bulb status
    update = read_bulbs();
    // Updated - write gradients
    if (update) write_gradients();
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

// Write gradients
void write_gradients() {
  uint16_t short_hues[2] = { bulb_hues[0], bulb_hues[1] };
  uint16_t long_hues[2] = { bulb_hues[0], bulb_hues[1] };
  uint8_t sats[2] = { bulb_sats[0], bulb_sats[1] };
  uint8_t vals[2] = { bulb_vals[0], bulb_vals[1] };
  uint16_t short_hue_range;
  uint16_t long_hue_range;
  uint16_t short_hue;
  uint16_t long_hue;
  uint8_t sat;
  uint8_t val;
  // Debug
  Serial.printf("\n%u: write_gradients():", millis());
  // Calculate short path hues going the short way around the color wheel allow hues greater than 360
  if (short_hues[0] > short_hues[1]) {
    short_hue_range = short_hues[0] - short_hues[1];
    if (short_hue_range >= 180) {
      short_hues[1] += 360;
      short_hue_range = short_hues[1] - short_hues[0];
    }
  } else {
    short_hue_range = short_hues[1] - short_hues[0];
    if (short_hue_range >= 180) {
      short_hues[0] += 360;
      short_hue_range = short_hues[0] - short_hues[1];
    }
  }
  // Calculate long path hues going the long way around the color wheel allow hues greater than 360
  if (long_hues[0] > long_hues[1]) {
    long_hue_range = long_hues[0] - long_hues[1];
    if (long_hue_range < 180) {
      long_hues[1] += 360;
      long_hue_range = long_hues[1] - long_hues[0];
    }
  } else {
    long_hue_range = long_hues[1] - long_hues[0];
    if (long_hue_range < 180) {
      long_hues[0] += 360;
      long_hue_range = long_hues[0] - long_hues[1];
    }
  }
  // Both bulbs are off ?
  if (!bulb_states[0] && !bulb_states[1]) {
    // Set brightnesses to 0
    vals[0] = vals[1] = 0;
  }
  // Bulb 0 off but bulb 1 is on ?
  else if (!bulb_states[0] && bulb_states[1]) {
    // Set brightness zero
    vals[0] = 0;
    // Copy hues and saturation from bulb 1 for single color fade to black
    sats[0] = sats[1];
    short_hues[0] = short_hues[1];
    long_hues[0] = long_hues[1];
  }
  // Bulb 1 off but bulb 0 is on ?
  else if (!bulb_states[1] && bulb_states[0]) {
    // Set brightness zero
    vals[1] = 0;
    // Copy hues and saturation from bulb 1 for single color fade to black
    sats[1] = sats[0];
    short_hues[1] = short_hues[0];
    long_hues[1] = long_hues[0];
  }
  // Both bulbs on ?
  else {
    // Bulb 0 is desaturated and bulb 1 is not ?
    if (sats[0] == 0 && sats[1] != 0) {
      // Use bulb 1's hue for bulb 0 so bulb 0's hue does not creep into the gradient
      short_hues[0] = short_hues[1];
      long_hues[0] = long_hues[1];
    }
    // Bulb 1 is desaturated and bulb 0 is not ?
    else if (sats[1] == 0 && sats[0] != 0) {
      // Use bulb 0's hue for bulb 1 so bulb 1's hue does not creep into the gradient
      short_hues[1] = short_hues[0];
      long_hues[1] = long_hues[0];
    }
  }
  // Debug
  Serial.printf(" short_hues = {%u, %u}, short_hue_range = %u", short_hues[0], short_hues[1], short_hue_range);
  Serial.printf(", long_hues = {%u, %u}, long_hue_range = %u", long_hues[0], long_hues[1], long_hue_range);
  Serial.printf(", sats = {%u, %u}, vals = {%u, %u}", sats[0], sats[1], vals[0], vals[1]);
  // Build gradients
  for (uint16_t i = 0; i < GRADIENT_COUNT; i++) {
    // Scale values for this entry
    short_hue = map(i, 0, GRADIENT_COUNT - 1, short_hues[0], short_hues[1]);
    long_hue = map(i, 0, GRADIENT_COUNT - 1, long_hues[0], long_hues[1]);
    sat = map(i, 0, GRADIENT_COUNT - 1, sats[0], sats[1]);
    val = map(i, 0, GRADIENT_COUNT - 1, vals[0], vals[1]);
    // Convert to rgb
    fast_hsv_to_rgb(short_hue, sat, val, &gradient_short_reds[i], &gradient_short_greens[i], &gradient_short_blues[i]);
    fast_hsv_to_rgb(long_hue, sat, val, &gradient_long_reds[i], &gradient_long_greens[i], &gradient_long_blues[i]);
#if 0
    // Debug
    Serial.printf("\n%u: sh = %u, s = %u, v = %u, sr = %u, sg= %u, sb=%u, lh = %u, s = %u, v = %u, lr = %u, lg= %u, lb=%u",
      i,
      short_hue, sat, val, gradient_short_reds[i], gradient_short_greens[i], gradient_short_blues[i],
      long_hue,  sat, val, gradient_long_reds[i],  gradient_long_greens[i],  gradient_long_blues[i]);
#endif
  }
  // Debug
  Serial.printf("\n%u: write_gradients(): DONE", millis());
}

// Write to pixels
void write_pixels() {
  // Data
  uint32_t index;
  bool sub;
  uint8_t r, g, b;
  // Debug
  //Serial.printf("\n(%u,%u)", noise_col, noise_row);
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
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, bulb_reds[1], bulb_greens[1], bulb_blues[1], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      break;

    // Short gradient ?
    case PIXEL_MODE_GRADIENT_SHORT:
      index = gradient_index;
      sub = gradient_sub;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        // Ensure all pixels get set to first color at start of gradient
        if (index < (PIXEL_COUNT / 2)) {
          r = gradient_short_reds[0];
          g = gradient_short_greens[0];
          b = gradient_short_blues[0];
        }
        // Ensure all pixels get set to last color at end of gradient
        else if (index > GRADIENT_COUNT + (PIXEL_COUNT / 2) - 1) {
          r = gradient_short_reds[GRADIENT_COUNT - 1];
          g = gradient_short_greens[GRADIENT_COUNT - 1];
          b = gradient_short_blues[GRADIENT_COUNT - 1];
        }
        // Use gradient color when in gradient
        else {
          r = gradient_short_reds[index - (PIXEL_COUNT / 2)];
          g = gradient_short_greens[index - (PIXEL_COUNT / 2)];
          b = gradient_short_blues[index - (PIXEL_COUNT / 2)];
        }
        pixels.set_pixel(1, r, g, b, 100, false);
        // Change direction ?
        if (index == 0) sub = false;
        else if (index == GRADIENT_COUNT + PIXEL_COUNT - 1) sub = true;
        // Next gradient index
        if (sub) index--;
        else index++;
      }
      pixels.end_transfer();
      interrupts();
      // Change direction ?
      if (gradient_index == 0) gradient_sub = false;
      else if (gradient_index == GRADIENT_COUNT + PIXEL_COUNT - 1) gradient_sub = true;
      // Next iteration
      if (gradient_sub) gradient_index--;
      else gradient_index++;
      break;

    // Long gradient ?
    case PIXEL_MODE_GRADIENT_LONG:
      index = gradient_index;
      sub = gradient_sub;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        // Ensure all pixels get set to first color at start of gradient
        if (index < (PIXEL_COUNT / 2)) {
          r = gradient_long_reds[0];
          g = gradient_long_greens[0];
          b = gradient_long_blues[0];
        }
        // Ensure all pixels get set to last color at end of gradient
        else if (index > GRADIENT_COUNT + (PIXEL_COUNT / 2) - 1) {
          r = gradient_long_reds[GRADIENT_COUNT - 1];
          g = gradient_long_greens[GRADIENT_COUNT - 1];
          b = gradient_long_blues[GRADIENT_COUNT - 1];
        }
        // Use gradient color when in gradient
        else {
          r = gradient_long_reds[index - (PIXEL_COUNT / 2)];
          g = gradient_long_greens[index - (PIXEL_COUNT / 2)];
          b = gradient_long_blues[index - (PIXEL_COUNT / 2)];
        }
        pixels.set_pixel(1, r, g, b, 100, false);
        // Change direction ?
        if (index == 0) sub = false;
        else if (index == GRADIENT_COUNT + PIXEL_COUNT - 1) sub = true;
        // Next gradient index
        if (sub) index--;
        else index++;
      }
      pixels.end_transfer();
      interrupts();
      // Change direction ?
      if (gradient_index == 0) gradient_sub = false;
      else if (gradient_index == GRADIENT_COUNT + PIXEL_COUNT - 1) gradient_sub = true;
      // Next iteration
      if (gradient_sub) gradient_index--;
      else gradient_index++;
      break;

    // Slow, short noise linear around noise image ?
    case PIXEL_MODE_NOISE_SLOW_SHORT:
      index = (noise_row * NOISE_WIDTH) + noise_col;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, gradient_short_reds[noise[index + p]], gradient_short_greens[noise[index + p]], gradient_short_blues[noise[index + p]], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      // Get ready for next iteration (work up and down and backwards and forwards around the edges of the noise)
      if (noise_row == 0) {
        noise_row_sub = false;
        if (noise_col_sub == true) {
          noise_col -= 1;
          if (noise_col == 0) noise_col_sub = false;
        } else {
          noise_col += 1;
          if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
        }
      } else if (noise_row >= NOISE_HEIGHT - 1) {
        noise_row_sub = true;
        if (noise_col_sub == true) {
          noise_col -= 1;
          if (noise_col == 0) noise_col_sub = false;
        } else {
          noise_col += 1;
          if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
        }
      }
      if (noise_row_sub) noise_row -= 1;
      else noise_row += 1;
      break;

    // Slow, long noise linear around noise image ?
    case PIXEL_MODE_NOISE_SLOW_LONG:
      index = (noise_row * NOISE_WIDTH) + noise_col;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, gradient_long_reds[noise[index + p]], gradient_long_greens[noise[index + p]], gradient_long_blues[noise[index + p]], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      // Get ready for next iteration (work up and down and backwards and forwards around the edges of the noise)
      if (noise_row == 0) {
        noise_row_sub = false;
        if (noise_col_sub == true) {
          noise_col -= 1;
          if (noise_col == 0) noise_col_sub = false;
        } else {
          noise_col += 1;
          if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
        }
      } else if (noise_row >= NOISE_HEIGHT - 1) {
        noise_row_sub = true;
        if (noise_col_sub == true) {
          noise_col -= 1;
          if (noise_col == 0) noise_col_sub = false;
        } else {
          noise_col += 1;
          if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
        }
      }
      if (noise_row_sub) noise_row -= 1;
      else noise_row += 1;
      break;

    // Fast, short noise diagonal around noise image ?
    case PIXEL_MODE_NOISE_FAST_SHORT:
      index = (noise_row * NOISE_WIDTH) + noise_col;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, gradient_short_reds[noise[index + p]], gradient_short_greens[noise[index + p]], gradient_short_blues[noise[index + p]], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      // Get ready for next iteration (bounce around the edges of the noise)
      if (noise_row == 0) noise_row_sub = false;
      else if (noise_row >= NOISE_HEIGHT - 1) noise_row_sub = true;
      if (noise_row_sub) noise_row -= 1;
      else noise_row += 1;
      if (noise_col == 0) noise_col_sub = false;
      else if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
      if (noise_col_sub) noise_col -= 1;
      else noise_col += 1;
      break;

    // Fast, long noise diagonal around noise image ?
    case PIXEL_MODE_NOISE_FAST_LONG:
    default:
      index = (noise_row * NOISE_WIDTH) + noise_col;
      noInterrupts();
      for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
        pixels.set_pixel(1, gradient_long_reds[noise[index + p]], gradient_long_greens[noise[index + p]], gradient_long_blues[noise[index + p]], 100, false);
      }
      pixels.end_transfer();
      interrupts();
      // Get ready for next iteration (bounce around the edges of the noise)
      if (noise_row == 0) noise_row_sub = false;
      else if (noise_row >= NOISE_HEIGHT - 1) noise_row_sub = true;
      if (noise_row_sub) noise_row -= 1;
      else noise_row += 1;
      if (noise_col == 0) noise_col_sub = false;
      else if (noise_col + PIXEL_COUNT >= NOISE_WIDTH - 1) noise_col_sub = true;
      if (noise_col_sub) noise_col -= 1;
      else noise_col += 1;
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
