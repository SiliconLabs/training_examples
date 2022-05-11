/***************************************************************************//**
 * @file app_rht_collector.c
 * @brief RHT Si7021 sensor collector functions
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
#include <string.h>
#include <stdbool.h>

#include "app_rht_collector.h"

#include "sl_board_control.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "sl_i2cspm_sensors_config.h" // This file name depends on your I2CSPM sensor instance name
#include "sl_si70xx.h"

#include "em_gpio.h"
#include "em_cmu.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define RHT_SENSOR_I2C_INSTANCE sl_i2cspm_sensors // Modify based on your
                                                      // I2CSPM instance name
#define RHT_SENSOR_COUNT            (2) // 2 sensors - RH and Temperature
#define RHT_SAMPLE_SIZE_BYTES       (4) // 1 sensor - sample is 32 bits
#define RHT_SAMPLE_BUFFER_SIZE      RHT_SAMPLES_PER_CYCLE // Local buffer size
#define RHT_SEND_BUFFER_SIZE_BYTES  (RHT_SAMPLES_PER_CYCLE * RHT_SENSOR_COUNT * RHT_SAMPLE_SIZE_BYTES) // Total number of bytes to transfer per sensor

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static bool transfer_is_in_progress(void);
static inline void copy_data_to_buffer(rht_sensor_data_t* buffer_pnt,
                                       size_t buffer_len);

static void enable_i2cspm_instance(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static rht_sensor_data_t rht_buffer[RHT_SAMPLES_PER_CYCLE]; // Local RHT buffer
static rht_sensor_data_t *sample_buffer; // Pointer to buffer

static uint8_t samples_collected = 0; // Sample count

static bool rht_collecting = false; // Flag for x transfer mode
static bool rht_init = false;      // Flag for initialized

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Verifies if the RHT sensor can be reached.
 *
 * @param[out] status
 ******************************************************************************/
sl_status_t app_rht_init(void)
{
  sl_status_t status = SL_STATUS_OK;

  // Initialize the I2CSPM instance
  //  Verify I2C instance state, by default the I2CSPM instance is initialized
  //  in autogen code.
  if (RHT_SENSOR_I2C_INSTANCE->EN && rht_init) {
    return SL_STATUS_ALREADY_INITIALIZED;
  } else {
    enable_i2cspm_instance();
  }

  // Verify if the Si7021 sensor is present
  status = sl_si70xx_init(RHT_SENSOR_I2C_INSTANCE, SI7021_ADDR);
  if (status == SL_STATUS_OK) {
    rht_init = true;
    rht_collecting = true;
  }

  return status;
}

/***************************************************************************//**
 * @brief
 *     Disables the RHT sensor and releases peripheral resources.
 *
 * @param[out] status
 ******************************************************************************/
void app_rht_deinit(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Disable I2CSPM peripheral instance
  I2C_Reset(RHT_SENSOR_I2C_INSTANCE);

  // Disable I2CSPM instance GPIOs
  GPIO_PinModeSet(SL_I2CSPM_SENSORS_SCL_PORT,
                  SL_I2CSPM_SENSORS_SCL_PIN,
                  gpioModeDisabled,
                  0);

  GPIO_PinModeSet(SL_I2CSPM_SENSORS_SDA_PORT,
                  SL_I2CSPM_SENSORS_SDA_PIN,
                  gpioModeDisabled,
                  0);

  // Disable I2C clock
  if (RHT_SENSOR_I2C_INSTANCE == I2C0) {
    CMU_ClockEnable(cmuClock_I2C0, false);
  } else if (RHT_SENSOR_I2C_INSTANCE == I2C1) {
    CMU_ClockEnable(cmuClock_I2C0, false);
  }

  rht_init = false;
  rht_collecting = false;
}

/***************************************************************************//**
 * @brief
 *     Flow control of RHT operation. It returns the RHT data when ready.
 *
 * @param[in] buf_pnt
 *     Pointer to application buffer that holds the RHT sensor data
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_rht_process_action(rht_sensor_data_t *buf_pnt)
{
  sl_status_t status = SL_STATUS_OK;

  if (rht_collecting) {
    status = app_rht_get_hum_and_temp(&rht_buffer[samples_collected].relative_humidity,
                                      &rht_buffer[samples_collected].temperature);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if(transfer_is_in_progress()) {
      return SL_STATUS_IN_PROGRESS;
    } else {
      copy_data_to_buffer(buf_pnt, RHT_SEND_BUFFER_SIZE_BYTES);

      samples_collected = 0;
      rht_collecting = true;
      return SL_STATUS_OK;
    }
  } else {
    return SL_STATUS_IDLE;
  }
}

/***************************************************************************//**
 * @brief
 *     Retrieves data from the RHT sensor, both RH and temperature.
 *
 * @param[out] rh_data
 *     Pointer to buffer that holds the relative humidity data.
 *
 * @param[out] temp_data
 *     Pointer to buffer that holds the temperature data.
 *
 * @param[out] status
 ******************************************************************************/
sl_status_t app_rht_get_hum_and_temp(uint32_t *rh_data, int32_t *temp_data)
{
  sl_status_t status = SL_STATUS_OK;

  status = sl_si70xx_measure_rh_and_temp(RHT_SENSOR_I2C_INSTANCE,
                                         SI7021_ADDR,
                                         rh_data,
                                         temp_data);
  if (status == SL_STATUS_OK) {
    samples_collected++;
  }

   return status;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Utility function to verify if all the requested samples have been acquired
 ******************************************************************************/
static bool transfer_is_in_progress(void)
{
  // Check the status of current transfer
  if (samples_collected < RHT_SAMPLES_PER_CYCLE) {
    return true;
  } else {
    sample_buffer = rht_buffer;
    rht_collecting = false;
    return false;
  }
}

/***************************************************************************//**
 * @brief
 *     Utility function to copy buffer_len RHT samples to the provided
 *     buffer.
 ******************************************************************************/
static inline void copy_data_to_buffer(rht_sensor_data_t* buffer_pnt,
                                       size_t buffer_len)
{
  memcpy(buffer_pnt, sample_buffer, buffer_len);
}

/***************************************************************************//**
 * @brief
 *     Initializes an I2CSPM instance. This is typically done by autogenerated
 *     code already.
 ******************************************************************************/
static void enable_i2cspm_instance(void)
{

#if SL_I2CSPM_SENSORS_SPEED_MODE == 0
#define SL_I2CSPM_SENSORS_HLR i2cClockHLRStandard
#define SL_I2CSPM_SENSORS_MAX_FREQ I2C_FREQ_STANDARD_MAX
#elif SL_I2CSPM_SENSORS_SPEED_MODE == 1
#define SL_I2CSPM_SENSORS_HLR i2cClockHLRAsymetric
#define SL_I2CSPM_SENSORS_MAX_FREQ I2C_FREQ_FAST_MAX
#elif SL_I2CSPM_SENSORS_SPEED_MODE == 2
#define SL_I2CSPM_SENSORS_HLR i2cClockHLRFast
#define SL_I2CSPM_SENSORS_MAX_FREQ I2C_FREQ_FASTPLUS_MAX
#endif

  I2CSPM_Init_TypeDef i2cspm_init_instance = {
    .port = SL_I2CSPM_SENSORS_PERIPHERAL,
    .sclPort = SL_I2CSPM_SENSORS_SCL_PORT,
    .sclPin = SL_I2CSPM_SENSORS_SCL_PIN,
    .sdaPort = SL_I2CSPM_SENSORS_SDA_PORT,
    .sdaPin = SL_I2CSPM_SENSORS_SDA_PIN,
    .i2cRefFreq = 0,
    .i2cMaxFreq = SL_I2CSPM_SENSORS_MAX_FREQ,
    .i2cClhr = SL_I2CSPM_SENSORS_HLR
  };

  CMU_ClockEnable(cmuClock_GPIO, true);

  I2CSPM_Init(&i2cspm_init_instance);
}
