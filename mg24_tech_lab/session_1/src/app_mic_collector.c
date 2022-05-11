/***************************************************************************//**
 * @file app_mic_collector.c
 * @brief I2S microphone collector functions
 * @version 1.0.0
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
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
 *
 * EVALUATION QUALITY
 * This code has been minimally tested to ensure that it builds with
 * the specified dependency versions and is suitable as a demonstration for
 * evaluation purposes only.
 * This code will be maintained at the sole discretion of Silicon Labs.
 *
 ******************************************************************************/
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdbool.h>
#include <string.h>

#include "app_mic_collector.h"

#include "sl_board_control.h"
#include "sl_mic.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// Microphone is ICS-43434 that uses 32 bit Word frames. each frame contains 24
// bits of data. The sl_mic driver uses only 16 bits of that data due to how it's
// configured. The driver is configured in stereo mode as well (WS pin toggles on each
// new frame. Data is left justified as required by the ICS-43434

#define MIC_AUDIO_CHANNELS            (1)   // 1 channel = left microphone data, 2 channel = left + right data
#define MIC_SAMPLE_SIZE_BYTES         (2)   // 1 sample is 16 bit
#define MIC_SAMPLE_BUFFER_SIZE        MIC_SAMPLES_PER_CYCLE  // Local buffer size
#define MIC_SEND_BUFFER_SIZE_BYTES    (MIC_SAMPLE_BUFFER_SIZE * MIC_SAMPLE_SIZE_BYTES) //Total number of bytes to transfer

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static void mic_buffer_ready_cb(const void *buffer, uint32_t n_frames);
static bool transfer_is_in_progress(void);
static inline void copy_data_to_buffer(int16_t* buffer_pnt,
                                       size_t buffer_len_bytes);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static int16_t mic_buffer[2 * MIC_SAMPLE_BUFFER_SIZE]; // Local microphone buffer
static int16_t *sample_buffer; // Pointer to buffer returned by microphone callback
static uint32_t frames; // Number of frames returned by microphone callback

static volatile bool mic_streaming = false; // Flag for streaming mode
static bool mic_transferring = false; // Flag for x transfer mode
static bool mic_init = false; // Flag for initialized
static bool mic_ready = false;  // Flow control - data is ready (streaming mode)

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Initializes the sl_mic driver.
 *
 * @param[in] sampling_frequency
 *     Sampling frequency in HZ
 *
 * @param[out] mic_status_t
 *     MIC_RETRIEVEING: Data retrieval was initiated
 *     MIC_READY: Data is ready
 ******************************************************************************/
sl_status_t app_mic_init(uint32_t sampling_frequency)
{
  sl_status_t status;

  // Microphone initialization
  status = sl_mic_init(sampling_frequency, MIC_AUDIO_CHANNELS);
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  mic_init = true;

  return status;
}

/***************************************************************************//**
 * @brief
 *     Enables/disables the microphone's VDD_MIC power net through the
 *     MIC_ENABLE GPIO. The VDD_MIC is powered through the VMMCU net (See board's
 *     schematic for details)
 *
 * @param[in] enable
 *     TRUE - Power microphones
 *     FALSE - Un-power microphones
 *
 * @param[out] status
 *     SL_STATUS_OK if there were no errors
 ******************************************************************************/
sl_status_t app_mic_enable(bool enable)
{
  sl_status_t status;

  if (enable) {
    status = sl_board_enable_sensor(SL_BOARD_SENSOR_MICROPHONE);
  } else {
    status = sl_board_disable_sensor(SL_BOARD_SENSOR_MICROPHONE);
  }

  return status;
}

/***************************************************************************//**
 * @brief
 *     Flow control of microphone operation. It returns the microphone data
 *     when ready.
 *
 * @param[in] buffer_pnt
 *     Pointer to application buffer that holds the microphone data
 *     buffer_len
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_mic_process_action(int16_t* buffer_pnt)
{
  if ( mic_transferring || mic_streaming ) {
    if(transfer_is_in_progress()) {
      return SL_STATUS_IN_PROGRESS;
    } else {
      copy_data_to_buffer(buffer_pnt, MIC_SEND_BUFFER_SIZE_BYTES);
      return SL_STATUS_OK;
    }
  } else {
    return SL_STATUS_IDLE;
  }
}

/***************************************************************************//**
 * @brief
 *     Stops any ongoing transfers, disables the microphone driver and the
 *     onboard microphones.
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_mic_stop(void)
{
  sl_status_t status = SL_STATUS_OK;

  // Microphone deinitialization
  status = sl_mic_deinit();
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Power-down microphone
  sl_board_disable_sensor(SL_BOARD_SENSOR_MICROPHONE);

  // Update flags
  mic_init = false;
  mic_streaming = false;
  mic_transferring = false;

  return status;
}

/***************************************************************************//**
 * @brief
 *     Starts the microphone operation in streaming mode. Not used in the
 *     collector code to save energy.
 *
 * @param[out] status
 ******************************************************************************/
sl_status_t app_mic_start_stream(void)
{
  sl_status_t status;

  // Check if microphone is initialized
  if (!mic_init) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Check if streaming is running
  if (mic_streaming) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  // Check if x transfer is running
  if (mic_transferring) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  // Power up microphone
  status = sl_board_enable_sensor(SL_BOARD_SENSOR_MICROPHONE);
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  // Start microphone sampling in stream mode
  status = sl_mic_start_streaming(mic_buffer, MIC_SAMPLE_BUFFER_SIZE,
                                  mic_buffer_ready_cb);
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  // Microphone streaming started
  mic_streaming = true;

  return status;
}

/***************************************************************************//**
 * @brief
 *     Starts the microphone operation in samples mode, to retrieve x number
 *     of samples. There's no callback so we need to poll the status
 *     of the transfer.
 *
 * @param[in] sample_cnt
 *     Number of samples to retrieve.
 *
 * @param[out] status
 ******************************************************************************/
sl_status_t app_mic_get_x_samples(uint32_t sample_cnt)
{
  sl_status_t status = SL_STATUS_OK;

  // Check if microphone is initialized
  if (!mic_init) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Check if x transfer is running
  if (mic_transferring) {
    return SL_STATUS_INVALID_STATE;
  }

  // Check if streaming is running
  if (mic_streaming) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  // Power up microphone
  status = sl_board_enable_sensor(SL_BOARD_SENSOR_MICROPHONE);
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  status = sl_mic_get_n_samples(mic_buffer, sample_cnt);
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  // Microphone transferring started
  frames = sample_cnt;
  mic_transferring = true;

  return status;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Callback function invoked by the sl_mic driver when a transfer is
 *     concluded in streaming mode.
 ******************************************************************************/
static void mic_buffer_ready_cb(const void *buffer, uint32_t n_frames)
{
  sample_buffer = (int16_t *)buffer;
  frames = n_frames;
  mic_ready = true;
}

/***************************************************************************//**
 * @brief
 *     Utility function to verify if a streaming of X sample transfer is still in
 *     progress.
 ******************************************************************************/
static bool transfer_is_in_progress(void)
{
  // Check the status of each transfer type
  if (sl_mic_sample_buffer_ready()) {
    sample_buffer = mic_buffer;
    mic_transferring = false;
    return false;
  } else if (mic_ready) {
    mic_ready = false;
    return false;
  } else {
    return true;
  }
}

/***************************************************************************//**
 * @brief
 *     Utility function to copy buffer_len microphone samples to the provided
 *     buffer.
 ******************************************************************************/
static inline void copy_data_to_buffer(int16_t* buffer_pnt,
                                       size_t buffer_len_bytes)
{
  memcpy(buffer_pnt, sample_buffer, buffer_len_bytes);
}
