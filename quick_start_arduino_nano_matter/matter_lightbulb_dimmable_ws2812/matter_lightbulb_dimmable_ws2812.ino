/*******************************************************************************
 * Matter dimmable lightbulb example for WS2812 LEDs
 *
 * The example shows how to create a dimmable lightbulb with the Arduino Matter 
 * API. 
 *
 * The example lets users control the onboard LED through Matter.
 * It's possible to switch the LED on/off and adjust the brightness as well.
 * The brightness level is displayed by lighting a proportion of the WS2812
 * LEDs and transitions between on, off and levels are animated.
 * The device has to be commissioned to a Matter hub first.
 *
 * Compatible boards:
 * - Arduino Nano Matter
 * - SparkFun Thing Plus MGM240P
 * - xG24 Explorer Kit
 * - xG24 Dev Kit
 *
 * Authors: Tamas Jozsi (Silicon Labs)
 *          Martin Looker (Silicon Labs)
 *
 *******************************************************************************
 * Copyright 2024 Silicon Laboratories Inc. www.silabs.com
 * 
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
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

// Defines
#define PIXEL_COUNT       96 // Number of WS2812 RGB LEDs, adjust as required
#define PIXEL_PERIOD      20 // Update period for pixels in ms
#define PIXEL_BRIGHTNESS 100 // Brightness percentage for on pixels, full white draws a lot of power!

// Data
MatterDimmableLightbulb matter_dimmable_bulb;
ezWS2812 pixels(PIXEL_COUNT);                                                                                                                                 // Pixel update period

// Function prototypes
void update_onboard_led(uint8_t brightness);

void setup()
{
  Serial.begin(115200);
  Matter.begin();
  matter_dimmable_bulb.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  update_onboard_led(0);

  // Initialise pixels to off
  pixels.begin();
  noInterrupts();
  pixels.set_all(0, 0, 0, 0);
  interrupts();

  Serial.println("Matter dimmable lightbulb");

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter device is not commissioned");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
  while (!Matter.isDeviceCommissioned()) {
    delay(200);
  }

  Serial.println("Waiting for Thread network...");
  while (!Matter.isDeviceThreadConnected()) {
    delay(200);
  }
  Serial.println("Connected to Thread network");

  Serial.println("Waiting for Matter device discovery...");
  while (!matter_dimmable_bulb.is_online()) {
    delay(200);
  }
  Serial.println("Matter device is now online");
}

void loop()
{
  static bool matter_lightbulb_last_state = false;
  bool matter_lightbulb_current_state = matter_dimmable_bulb.get_onoff();

  // If the current state is ON and the previous was OFF - set the LED to the last brightness value
  if (matter_lightbulb_current_state && !matter_lightbulb_last_state) {
    matter_lightbulb_last_state = matter_lightbulb_current_state;
    update_onboard_led(matter_dimmable_bulb.get_brightness());
    Serial.printf("Bulb ON, brightness: %u%%\n", matter_dimmable_bulb.get_brightness_percent());
  }

  // If the current state is OFF and the previous was ON - turn off the LED
  if (!matter_lightbulb_current_state && matter_lightbulb_last_state) {
    matter_lightbulb_last_state = matter_lightbulb_current_state;
    update_onboard_led(0);
    Serial.println("Bulb OFF");
  }

  // If the brightness changes update the LED brightness accordingly
  static uint8_t last_brightness = matter_dimmable_bulb.get_brightness();
  uint8_t curr_brightness = matter_dimmable_bulb.get_brightness();
  if (last_brightness != curr_brightness) {
    update_onboard_led(curr_brightness);
    last_brightness = curr_brightness;
    Serial.printf("Bulb brightness changed to %u%%\n", matter_dimmable_bulb.get_brightness_percent());
  }

  // Update pixels to required brightness level
  static uint32_t now_millis = millis(); 
  static uint32_t pixel_millis = now_millis;   
  static uint16_t current_pixels = 0;
  static uint16_t target_pixels = 0;
  // Get current time
  now_millis = millis();
  // Pixels timer fired ?
  if (now_millis - pixel_millis >= PIXEL_PERIOD) {
    // Restart timer
    pixel_millis = now_millis;
    // Is bulb on ?
    if (matter_lightbulb_current_state) {
      // Calculate target pixels (scaling 0-254 brightness to number of pixels)
      target_pixels = map(curr_brightness, 0, 254, 0, PIXEL_COUNT);
    }
    // Is bulb off ?
    else {
      // Zero target pixels for off
      target_pixels = 0;
    }
    // Need to increase current pixels ?
    if (current_pixels < target_pixels) current_pixels++;
    // Need to decrease current pixels ?
    if (current_pixels > target_pixels) current_pixels--;
    // Update pixels
    noInterrupts();
    for (uint16_t p = 0; p < PIXEL_COUNT; p++) {
      // On pixel ?
      if (p < current_pixels) pixels.set_pixel(1, 0xFF, 0xFF, 0xFF, PIXEL_BRIGHTNESS, false);
      // Off pixel ?
      else pixels.set_pixel(1, 0x00, 0x00, 0x00, PIXEL_BRIGHTNESS, false);
    }
    pixels.end_transfer();
    interrupts();    
  }
}

void update_onboard_led(uint8_t brightness)
{
  // If our built-in LED is active LOW, we need to invert the brightness value
  if (LED_BUILTIN_ACTIVE == LOW) {
    analogWrite(LED_BUILTIN, 255 - brightness);
  } else {
    analogWrite(LED_BUILTIN, brightness);
  }
}
