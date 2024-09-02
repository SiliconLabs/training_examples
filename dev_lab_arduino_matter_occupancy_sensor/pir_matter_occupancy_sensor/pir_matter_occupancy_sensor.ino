/******************************************************************************
 * Copyright 2024 Silicon Laboratories Inc. www.silabs.com
 ******************************************************************************
 * Matter PIR Occupancy Sensor - Training Example
 *
 * The example shows how to create an occupancy sensor with the Arduino Matter
 * API and publishes occupancy information through it. 
 *
 * The occupancy state is true when the D6 input is high and unoccupied when 
 * low. 
 * 
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
 ****************************************************************************** 
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
 ******************************************************************************
 * EXPERIMENTAL QUALITY
 * 
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 * 
 *****************************************************************************/

#include <Matter.h>
#include <MatterOccupancy.h>

#define OUTPUT_OCCUPANCY LED_BUILTIN_1
#define INPUT_OCCUPANCY D6
#define INPUT_PERIOD 20

void update_output();

MatterOccupancy matter_occupancy_sensor;
uint32_t now_millis;
uint32_t input_millis;
uint8_t input_occupancy = 0b10101010;

void setup() {
  Serial.begin(115200);
  Matter.begin();
  matter_occupancy_sensor.begin();

  Serial.println("WW24 PIR Matter Occupancy Sensor");

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
  while (!matter_occupancy_sensor.is_online()) {
    delay(200);
  }
  Serial.println("Matter device is now online");

  // Setup inputs
  pinMode(INPUT_OCCUPANCY, INPUT);
  // Setup outputs
  pinMode(OUTPUT_OCCUPANCY, OUTPUT);
  update_output();
  // Start loop timers
  now_millis = input_millis = millis();
}

void loop() {
  // Update time
  now_millis = millis();
  // Input timer fired
  if (now_millis - input_millis >= INPUT_PERIOD) {
    uint8_t old_input;
    // Restart timer
    input_millis = now_millis;
    // Note old debounce value
    old_input = input_occupancy;
    // Shift debounce
    input_occupancy <<= 1;
    // Read input
    if (digitalRead(INPUT_OCCUPANCY) == HIGH) input_occupancy |= 1;
    // Debounce has changed?
    if (input_occupancy != old_input) {
      // Unoccupied ?
      if (input_occupancy == 0) {
        // Update matter state
        matter_occupancy_sensor = false;
        Serial.println("PIR - Unoccupied");
        update_output();
      }
      // Occupied ?
      else if (input_occupancy == 0b11111111) {
        // Update matter state
        matter_occupancy_sensor = true;
        Serial.println("PIR - Occupied");
        update_output();
      }
    }
  }
}

void update_output() {
  // Apply matter state to output
  if (matter_occupancy_sensor.get_occupancy()) {
    digitalWrite(OUTPUT_OCCUPANCY, LED_BUILTIN_ACTIVE);
  } else {
    digitalWrite(OUTPUT_OCCUPANCY, LED_BUILTIN_INACTIVE);
  }
}