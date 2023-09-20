/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
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
 ******************************************************************************/
// Includes --------------------------------------------------------------------
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"

// Constants
#include "constants.h"
// Log - software component
#include "app_log.h"
// Simple LED - software component
#include "sl_simple_led_instances.h" /* 0=red, 1=green, 2=blue */
// Simple Button - software component
#include "sl_simple_button_instances.h"
// Sleep Timer - software component
#include "sl_sleeptimer.h"
// Magic Wand
#include "magic_wand.h"

// Function prototypes ---------------------------------------------------------
static bool process_scan_response(sl_bt_evt_scanner_legacy_advertisement_report_t *pResp);
static void app_tick_timer_cb(sl_sleeptimer_timer_handle_t *tick_timer_cb, void *data);
static void button_loop(void);

// Local data ------------------------------------------------------------------
// Sleep timer
static sl_sleeptimer_timer_handle_t tick_timer;
static uint8_t tick_counter = 0;
// State
static uint8_t state = APP_STATE_NONE;
static uint8_t button_state = APP_STATE_NONE;
// Connection data
static uint8_t _conn_handle = 0xFF;
static uint32_t _service_handle = 0;
static uint16_t _char_handle = 0;
// Blinky service UUID: de8a5aac-a99b-c315-0c80-60d4cbb51224
const uint8_t serviceUUID[16] =
{ 0x24, 0x12, 0xb5, 0xcb, 0xd4, 0x60, 0x80, 0x0c, 0x15, 0xc3, 0x9b, 0xa9, 0xac, 0x5a, 0x8a, 0xde };

// LED Control Characteristic UUID: 5b026510-4088-c297-46d8-be6c736a087a
const uint8_t charUUID[16] =
{ 0x7a, 0x08, 0x6a, 0x73, 0x6c, 0xbe, 0xd8, 0x46, 0x97, 0xc2, 0x88, 0x40, 0x10, 0x65, 0x02, 0x5b };
// Strings
char app_states[7][15];

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  app_log("\nBluetooth - SoC Wandy\r\n");
  app_log("app_init()\r\n");
  // Initialise strings
  strcpy(app_states[0], "NONE");
  strcpy(app_states[1], "SCAN");
  strcpy(app_states[2], "SERVICE");
  strcpy(app_states[3], "CHARACTERISTIC");
  strcpy(app_states[4], "CONNECTED");
  strcpy(app_states[5], "OFF");
  strcpy(app_states[6], "ON");
  // Start sleep timer
  (void) sl_sleeptimer_start_periodic_timer_ms(&tick_timer,
  TICK_TIMER_MS, app_tick_timer_cb, (void*) NULL, 0, 0);
  // Initialise magic wand
  magic_wand_init();
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
  // Process magic wand
  magic_wand_loop();
  // Process button
  button_loop();
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  // Which event type ?
  switch (SL_BT_MSG_ID(evt->header))
  {
  case sl_bt_evt_system_boot_id:
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    //
    app_log("sl_bt_on_event(sl_bt_evt_system_boot_id)\r\n");
    // Set state
    sc = app_set_state(APP_STATE_SCAN);
    // Start scanning
    sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_generic);
    app_log("0x%04x = sl_bt_scanner_start()\r\n", (uint16_t ) sc);
    break;

  case sl_bt_evt_scanner_legacy_advertisement_report_id:
    // -------------------------------
    // This event indicates an advertisement report has been received.
    //
    // Scan response we are looking for ?
    if (process_scan_response(&evt->data.evt_scanner_legacy_advertisement_report))
    {
      app_log("sl_bt_on_event(sl_bt_evt_scanner_legacy_advertisement_report_id)\r\n");
      // Open connection
      sc = sl_bt_connection_open(evt->data.evt_scanner_scan_report.address,
          evt->data.evt_scanner_scan_report.address_type, sl_bt_gap_1m_phy, &_conn_handle);
      app_log("0x%04x = sl_bt_connection_open()\r\n", (uint16_t ) sc);
      // Success ?
      if (SL_STATUS_OK == sc)
      {
        // Stop scanning
        sc = sl_bt_scanner_stop();
        app_log("0x%04x = sl_bt_scanner_stop()\r\n", (uint16_t ) sc);
      }
    }
    break;

  case sl_bt_evt_connection_opened_id:
    // -------------------------------
    // This event indicates a connection has been opened.
    //
    app_log("sl_bt_on_event(sl_bt_evt_connection_opened_id)\r\n");
    // Set state
    sc = app_set_state(APP_STATE_SERVICE);
    // Discover services
    sc = sl_bt_gatt_discover_primary_services_by_uuid(_conn_handle, 16, serviceUUID);
    app_log("0x%04x = sl_bt_gatt_discover_primary_services_by_uuid()\r\n", (uint16_t ) sc);
    break;

  case sl_bt_evt_gatt_service_id:
    // -------------------------------
    // This event indicates a service has been discovered.
    //
    // Correct length ?
    if (evt->data.evt_gatt_service.uuid.len == 16)
    {
      // Correct service ?
      if (memcmp(serviceUUID, evt->data.evt_gatt_service.uuid.data, 16) == 0)
      {
        app_log("sl_bt_on_event(sl_bt_evt_gatt_service_id)\r\n");
        // Save handle
        _service_handle = evt->data.evt_gatt_service.service;
      }
    }
    break;

  case sl_bt_evt_gatt_characteristic_id:
    // -------------------------------
    // This event indicates a characteristic has been discovered.
    //
    // Correct length ?
    if (evt->data.evt_gatt_characteristic.uuid.len == 16)
    {
      // Correct characteristic ?
      if (memcmp(charUUID, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0)
      {
        app_log("sl_bt_on_event(sl_bt_evt_gatt_characteristic_id)\r\n");
        // Save handle
        _char_handle = evt->data.evt_gatt_characteristic.characteristic;
      }
    }
    break;

  case sl_bt_evt_gatt_procedure_completed_id:
    // -------------------------------
    // This event indicates a GATT procedure has completed.
    //
    // Which state are we in ?
    switch (state)
    {
    case APP_STATE_SERVICE:
      app_log("sl_bt_on_event(sl_bt_evt_gatt_procedure_completed_id)\r\n");
      // Found a service handle ?
      if (_service_handle > 0)
      {
        // Next state
        sc = app_set_state(APP_STATE_CHARACTERISTIC);
        // Search for characteristics
        sc = sl_bt_gatt_discover_characteristics(_conn_handle, _service_handle);
        app_log("0x%04x = sl_bt_gatt_discover_characteristics()\r\n", (uint16_t ) sc);

      }
      // Service not found ?
      else
      {
        // Disconnect
        sc = sl_bt_connection_close(_conn_handle);
        app_log("0x%04x = sl_bt_connection_close()\r\n", (uint16_t ) sc);
      }
      break;

    case APP_STATE_CHARACTERISTIC:
      app_log("sl_bt_on_event(sl_bt_evt_gatt_procedure_completed_id)\r\n");
      // Found characteristic handle ?
      if (_char_handle > 0)
      {
        // Next state
        sc = app_set_state(APP_STATE_CONNECTED);
      }
      // Didn't find characteristic handle ?
      else
      {
        // Disconnect
        sc = sl_bt_connection_close(_conn_handle);
        app_log("0x%04x = sl_bt_connection_close()\r\n", (uint16_t ) sc);

      }
      break;

    case APP_STATE_OFF:
      app_log("sl_bt_on_event(sl_bt_evt_gatt_procedure_completed_id)\r\n");
      break;

    case APP_STATE_ON:
      app_log("sl_bt_on_event(sl_bt_evt_gatt_procedure_completed_id)\r\n");
      break;

    default:
      break;
    }
    break;

  case sl_bt_evt_connection_closed_id:
    // -------------------------------
    // This event indicates a connection is closed.
    //
    app_log("sl_bt_on_event(sl_bt_evt_connection_closed_id)\r\n");
    // Reset connection data
    _conn_handle = 0xFF;
    _service_handle = 0;
    _char_handle = 0;
    // Set state
    sc = app_set_state(APP_STATE_SCAN);
    // Start scanning
    sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_generic);
    app_log("0x%04x = sl_bt_scanner_start()\r\n", (uint16_t ) sc);
    break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
  default:
    break;
  }
}

/**************************************************************************//**
 * Scan response event handler.
 *
 * Decoding advertising packets is done here. The list of AD types can be found
 * at: https://www.bluetooth.com/specifications/assigned-numbers/Generic-Access-Profile
 *****************************************************************************/
static bool process_scan_response(sl_bt_evt_scanner_legacy_advertisement_report_t *pResp)
{
  uint8_t i = 0, ad_len, ad_type;
  bool ad_name_found = false;
  char ad_name[32];

  // Loop through advertising packets
  while (i < (pResp->data.len - 1))
  {
    // Extract length and type
    ad_len = pResp->data.data[i];
    ad_type = pResp->data.data[i + 1];
    // Type 0x08 = Shortened Local Name
    // Type 0x09 = Complete Local Name
    if (ad_type == 0x08 || ad_type == 0x09)
    {
      // Copy name
      memcpy(ad_name, &(pResp->data.data[i + 2]), ad_len - 1);
      ad_name[ad_len - 1] = 0;
      // Name matches ?
      if (strcmp(AD_NAME, ad_name) == 0)
      {
        // Note we found the name we were looking for
        ad_name_found = true;
        // Log
        //app_log(
        //		"process_scan_response(%02x:%02x:%02x:%02x:%02x:%02x, %d, \"%s\")\r\n",
        //		pResp->address.addr[0], pResp->address.addr[1],
        //		pResp->address.addr[2], pResp->address.addr[3],
        //		pResp->address.addr[4], pResp->address.addr[5],
        //		pResp->rssi, ad_name);
      }
    }
    // Jump to next AD record
    i = i + ad_len + 1;
  }

  return ad_name_found;
}

/**************************************************************************//**
 * Tick timer event handler.
 *****************************************************************************/
static void app_tick_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;

  // Increment counter
  tick_counter++;
  // Which state ?
  switch (state)
  {
  case APP_STATE_ON:
    // Turn on led0/green
    sl_led_turn_off(&sl_led_led0);
    sl_led_turn_on(&sl_led_led1);
    break;
  case APP_STATE_OFF:
    // Turn on led1/red
    sl_led_turn_on(&sl_led_led0);
    sl_led_turn_off(&sl_led_led1);
    break;
  case APP_STATE_CONNECTED:
    // Turn on led0+led1/yellow
    sl_led_turn_on(&sl_led_led0);
    sl_led_turn_on(&sl_led_led1);
    break;
  case APP_STATE_CHARACTERISTIC:
  case APP_STATE_SERVICE:
  case APP_STATE_SCAN:
    // Flash led0+led1/yellow
    if (tick_counter & 0x01)
    {
      sl_led_turn_off(&sl_led_led0);
      sl_led_turn_off(&sl_led_led1);
    }
    else
    {
      sl_led_turn_on(&sl_led_led0);
      sl_led_turn_on(&sl_led_led1);
    }
    break;
  case APP_STATE_NONE:
  default:
    // Alternate led0+led1/red+green
    if (tick_counter & 0x01)
    {
      sl_led_turn_on(&sl_led_led0);
      sl_led_turn_off(&sl_led_led1);
    }
    else
    {
      sl_led_turn_on(&sl_led_led0);
      sl_led_turn_off(&sl_led_led1);
    }
    break;
  }
}

/**************************************************************************//**
 * Push button event handler
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  uint8_t btn = 0xFF;
  uint8_t btn_state = 0xFF;

  // Get state
  btn_state = sl_button_get_state(handle);
  // Button 0 ?
  if (handle == &sl_button_btn0)
  {
    btn = 0;
  }
  // Button 1 ?
  else if (handle == &sl_button_btn1)
  {
    btn = 1;
  }
  app_log("sl_button_on_change(%d, %d)\r\n", btn, btn_state);
  // Button down ?
  if (1 == btn_state)
  {
    // Valid button ?
    if (btn != 0xFF)
    {
      // Button 0 - attempt to turn off
      if (0 == btn)
      {
        button_state = APP_STATE_OFF;
      }
      // Button 1 - attempt to turn on
      else if (1 == btn)
      {
        button_state = APP_STATE_ON;
      }
    }
  }
}

/**************************************************************************//**
 * Push button loop processing
 *****************************************************************************/
static void button_loop(void)
{
  if (APP_STATE_ON == button_state || APP_STATE_OFF == button_state)
  {
    (void) app_set_state(button_state);
  }
  button_state = APP_STATE_NONE;
}

/**************************************************************************//**
 * Application set state
 *****************************************************************************/
sl_status_t app_set_state(uint8_t new_state)
{
  sl_status_t sc = SL_STATUS_FAIL;

  // Valid state ?
  if (new_state <= APP_STATE_ON)
  {
    // Trying to go to on or off
    if (APP_STATE_ON == new_state || APP_STATE_OFF == new_state)
    {
      // In a connected state ?
      if (state >= APP_STATE_CONNECTED)
      {
        // OK to change state
        sc = SL_STATUS_OK;
      }
    }
    // Not trying to go on or off ?
    else
    {
      // OK to change state
      sc = SL_STATUS_OK;
    }
  }
  // OK to change state ?
  if (SL_STATUS_OK == sc)
  {
    sl_status_t sc_tx = SL_STATUS_FAIL;
    uint8_t data;

    // Note new state
    state = new_state;
    app_log("0x%04x = app_set_state(%s)\r\n", (uint16_t ) sc, app_states[state]);
    // Moved to on state ?
    if (APP_STATE_ON == state)
    {
      // Transmit data
      data = 1;
      sc_tx = sl_bt_gatt_write_characteristic_value(_conn_handle, _char_handle, 1, &data);
      app_log("0x%04x = sl_bt_gatt_write_characteristic_value(%d)\r\n", (uint16_t ) sc_tx, data);
    }
    // Moved to off state ?
    else if (APP_STATE_OFF == state)
    {
      // Transmit data
      data = 0;
      sc_tx = sl_bt_gatt_write_characteristic_value(_conn_handle, _char_handle, 1, &data);
      app_log("0x%04x = sl_bt_gatt_write_characteristic_value(%d)\r\n", (uint16_t ) sc_tx, data);
    }
  }

  return sc;
}

/**************************************************************************//**
 * Application get state
 *****************************************************************************/
uint8_t app_get_state(void)
{
  return state;
}
