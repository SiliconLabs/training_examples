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
 ******************************************************************************
 *
 * EVALUATION QUALITY
 * This code has been minimally tested to ensure that it builds with
 * the specified dependency versions and is suitable as a demonstration for
 * evaluation purposes only.
 * This code will be maintained at the sole discretion of Silicon Labs.
 *
 ******************************************************************************/

#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "app_log.h"
#include "sl_mic.h"
#include "sl_board_control.h"
#include <math.h>
#include "em_core.h"
#include "kb.h"
#include "sl_sleeptimer.h"

// This would be your model index
#define MODEL_INDEX KB_MODEL_GUITAR_NOTE_RECOGNITION_DEMO_RANK_0_INDEX

#define MIC_SAMPLE_RATE 16000u
#define MIC_N_CHANNELS 1u
#define MIC_FRAMES_PER_CALLBACK 1024u
#define BYTES_PER_FRAME (2 * MIC_N_CHANNELS)
#define FIFO_SIZE (25* MIC_FRAMES_PER_CALLBACK * MIC_N_CHANNELS)

// labels mapping model inference values to note names
const char *labels[] = {"Unknown","A","B","D","E","E1","G", "Unknown"};

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// connection to client
static uint8_t connection;

// true if client is connected and has requested notifications for inferences
static bool subscribed = false;

// Buffer to be passed to I2S Microphone Driver
static int16_t buffer[2 * MIC_FRAMES_PER_CALLBACK * MIC_N_CHANNELS];

// FIFO used to buffer data from I2S Microphone Driver
static int16_t fifo_data[FIFO_SIZE];

// FIFO state variables
static volatile uint32_t fifo_head, fifo_tail;

// statistics from interrupt callback
static volatile uint32_t overrun, callbacks;

// if notification queue is full, save last attempt for  retry
int last_attempt = 0;

// statistics for printing performance
uint32_t ticks_processing, tick_start, max_lag, notifications_sent, samples_processed, notifications_attempted;

/* this will be in interrupt context */
void callback(const int16_t *data, uint32_t n_frames) {
  callbacks++;
  if ((fifo_head + n_frames) > (fifo_tail + FIFO_SIZE)) { // if insufficient space to store incoming data drop entire block
      overrun++;
      return;
  }
  uint32_t head = fifo_head % FIFO_SIZE;
  if(head + n_frames > FIFO_SIZE) {              // this should never happen since FIFO length is multiple of sample set size
      uint32_t crossover = FIFO_SIZE - fifo_head;
      for(uint32_t i = 0; i < crossover; i++) {
          fifo_data[head+i] = data[i];
      }
      for(uint32_t i = crossover; i < n_frames; i++) {
          fifo_data[i-crossover] = data[i];
      }
      fifo_head = n_frames - crossover;
  } else {                                      // normal simple copy
      for(uint32_t i = 0; i < n_frames; i++) {
          fifo_data[head+i] = data[i];
      }
      fifo_head += n_frames;                   // advance FIFO head
  }
}


sl_status_t send_notification(int inference) {
  sl_status_t sc;
  if(7 == inference) return SL_STATUS_NO_MORE_RESOURCE;
  if((inference >= 0) && (sizeof(labels) > inference*sizeof(char*))) {
      sc = sl_bt_gatt_server_send_notification(connection, gattdb_inference, strlen(labels[inference]), (uint8_t*)labels[inference]);
      return sc;
  }
  app_log_error("Inference out of bounds (%d)\r\n",inference);
  return SL_STATUS_ABORT;
}

void process_model(void) {
  sl_status_t sc;
  int ret;
  uint32_t head, tail, to_send;
  if (last_attempt) { // if last attempt to notify failed, retry
      notifications_attempted++;
      sc = send_notification(last_attempt);
      if (SL_STATUS_OK == sc) {
          last_attempt = 0;
          notifications_sent++;
      }
  }
  CORE_CRITICAL_SECTION(
      head = fifo_head;
      tail = fifo_tail;
  )
  to_send = head - tail;
  if(to_send > max_lag) {
      max_lag = to_send;
  }
  if(0 == to_send) return;
  head %= FIFO_SIZE;
  tail %= FIFO_SIZE;
  uint32_t before, after;
  before = sl_sleeptimer_get_tick_count();
  for(uint32_t i = 0; i < to_send; i++) {
      ret = kb_run_model((SENSOR_DATA_T*)&fifo_data[(tail+i)%FIFO_SIZE], 1, MODEL_INDEX);
      if(ret > 0) {
          notifications_attempted++;
          sc = send_notification(ret);
          if(SL_STATUS_OK == sc) { // if last attempt failed but current attempt with new value succeeds last_attempt is stale, discard
              last_attempt = 0;
              notifications_sent++;
          }
          if(SL_STATUS_NO_MORE_RESOURCE ==sc) { // message buffer is currently full, most recently attempted for retry
              last_attempt = ret;
          }
          kb_reset_model(0);
      }
  }
  after = sl_sleeptimer_get_tick_count();
  ticks_processing += after - before;
  CORE_CRITICAL_SECTION(
      fifo_tail += to_send;
  )
  samples_processed += to_send;
}

void processing_start(void) {
  sl_status_t sc;
  kb_reset_model(0);
  fifo_head = 0;
  fifo_tail = 0;
  overrun = 0;
  max_lag = 0;
  ticks_processing = 0;
  samples_processed = 0;
  notifications_attempted = 0;
  notifications_sent = 0;
  app_log_info("Collection, analysis and notification starting\r\n");
  tick_start = sl_sleeptimer_get_tick_count();
  sc = sl_mic_start_streaming(buffer, MIC_FRAMES_PER_CALLBACK, (sl_mic_buffer_ready_callback_t)callback);
  app_assert_status(sc);
}

void processing_stop(void) {
  sl_status_t sc;
  uint32_t runtime = sl_sleeptimer_get_tick_count() - tick_start;
  sc = sl_mic_stop();
  app_assert_status(sc);
  app_log_info("Collection, analysis and notification stopped\r\n");
  app_log_info("callbacks: %d, overrun: %d, max_lag: %d\r\n",callbacks, overrun, max_lag);
  app_log_info("Samples processed: %d, notifications attempted: %d, notifications sent: %d\r\n",samples_processed,notifications_attempted,notifications_sent);
  app_log_info("%d ticks of analysis, %d total ticks, %d %% spent processing data\r\n",ticks_processing,runtime,100*ticks_processing/runtime);
}

void advertising_start(void) {
  sl_status_t sc;
  sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
  app_assert_status(sc);
  app_log_info("Advertising started\r\n");
}
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
  sl_status_t sc;
  app_log_info("SensiML + BLE example\r\n");
  sc = sl_board_enable_sensor(SL_BOARD_SENSOR_MICROPHONE);
  app_assert_status(sc);
  sc = sl_mic_init(MIC_SAMPLE_RATE, MIC_N_CHANNELS);
  app_assert_status(sc);
  fifo_head = 0;
  fifo_tail = 0;
  overrun = 0;
  kb_model_init();
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
  process_model();
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

      advertising_start();
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
#define ED evt->data.evt_connection_opened
      connection = ED.connection;
      app_log_info("Connection opened\r\n");
      break;
#undef ED
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
#define ED evt->data.evt_connection_closed
      connection = 0xff;
      if(subscribed) {
          processing_stop();
      }
#undef ED
      app_log_info("Connection closed\r\n");
      advertising_start();
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
#define ED evt->data.evt_gatt_server_characteristic_status
      if(gattdb_inference != ED.characteristic) break;
      if(1 != ED.status_flags) break;
      if(subscribed == ED.client_config_flags) break;
      subscribed = ED.client_config_flags;
      if(subscribed) {
          processing_start();
      } else {
          processing_stop();
      }
      break;
#undef ED

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
