/***************************************************************************//**
 * @file app_mic_collector.c
 * @brief IMU sensor collector functions
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

#include "app_imu_collector.h"
#include "sl_icm20689.h"

#include "sl_board_control.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_eusart.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

#define IMU_SENSOR_COUNT            (2) //Accelerometer and Gyroscope
#define IMU_SENSOR_AXIS_COUNT       (3) //3-axis per sensor
#define IMU_AXIS_SAMPLE_SIZE_BYTES  (2) // 1 axis - sample is 16 bits
#define IMU_SAMPLE_BUFFER_SIZE      IMU_SAMPLES_PER_CYCLE // Local buffer size
#define IMU_SEND_BUFFER_SIZE_BYTES  (IMU_SAMPLES_PER_CYCLE * IMU_SENSOR_COUNT * IMU_SENSOR_AXIS_COUNT * IMU_AXIS_SAMPLE_SIZE_BYTES) // Total number of bytes to transfer per sensor

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static float get_imu_odr(imu_accel_gyro_odr_t odr);
static bool transfer_is_in_progress(void);
static inline void copy_data_to_buffer(imu_6_axis_data_t *buffer_pnt,
                                       size_t buffer_len);

static void config_imu_int_gpio(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static imu_6_axis_data_t imu_buffer[IMU_SAMPLE_BUFFER_SIZE]; // Local IMU buffer
static imu_6_axis_data_t *sample_buffer; // Pointer to buffer

static uint8_t samples_collected = 0; // Sample count

static bool imu_collecting = false; // Flag for x transfer mode
static bool imu_init = false;      // Flag for initialized

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Initializes the sl_mic driver. IMU is sent to sleep immediately after to
 *     reduce current consumption.
 *
 * @param[in] sample_rate
 *     Sampling frequency for the IMU
 *
 * @param[out] mic_status_t
 *     MIC_RETRIEVEING: Data retrieval was initiated
 *     MIC_READY: Data is ready
 ******************************************************************************/
sl_status_t app_imu_init(imu_accel_gyro_odr_t sample_rate)
{
  sl_status_t status;
  app_imu_status_t imu_status;

  //IMU Initialization
  imu_status = sl_imu_get_state();

  if (imu_status != IMU_DISABLED) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  // IMU INT pin is enabled in open-drain mode, active low, 50us pulse INT
  // status is cleared by reading the INT_STATUS reg
  status = sl_imu_init();
  if ( status != SL_STATUS_OK ) {
    return status;
  }

  // Default configurations is:
  // 6 axis - normal mode
  // Accelerometer:
  //   3-db BW: 1046 Hz
  //   Averaging: Don't care - 1x
  //   Full scale: 2G
  // Gyroscope:
  //   3-db BW: 41 Hz
  //   Averaging: 1x
  //   Full scale: +- 250dps
  // RAW data ready interrupt enabled
  sl_imu_configure(get_imu_odr(sample_rate));

  // Send IMU to sleep after initialization to save energy
  app_imu_sleep(true);

  // Configure the INT GPIO and enable it's external interrupt
  config_imu_int_gpio();

  imu_init = true;
  imu_collecting = true;

  return status;
}

/***************************************************************************//**
 * @brief
 *     Flow control of microphone operation. It returns the microphone data
 *     when ready.
 *
 * @param[in] buf_pnt
 *     Pointer to application buffer that holds the IMU data
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_imu_process_action(imu_6_axis_data_t *buf_pnt)
{
  sl_status_t status = SL_STATUS_OK;

  if (imu_collecting) {
    status = app_imu_get_data(imu_buffer[samples_collected].orientation,
                              imu_buffer[samples_collected].acceleration);

    if (status != SL_STATUS_OK) {
      return status;
    }

    if(transfer_is_in_progress()) {
      return SL_STATUS_IN_PROGRESS;
    } else {
      copy_data_to_buffer(buf_pnt, IMU_SEND_BUFFER_SIZE_BYTES);

      samples_collected = 0;
      return SL_STATUS_OK;
    }
  } else {
    return SL_STATUS_IDLE;
  }
}

/***************************************************************************//**
 * @brief
 *     Stops and disables the IMU.
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_imu_stop(void)
{
  sl_status_t status = SL_STATUS_OK;
  app_imu_status_t imu_status;

  //IMU state check
  imu_status = sl_imu_get_state();

  if (imu_status == IMU_STATE_DISABLED) {
    return SL_STATUS_OK;
  }

  status = sl_imu_deinit(); //Note that sl_imu_deinit doesn't really modify the
                            //IMU it's just an internal flag

  return status;

  // Update flags
  imu_init = false;
  imu_collecting = false;

  return status;
}

/***************************************************************************//**
 * @brief
 *     Sends the IMU to sleep.
 *
 * @param[in] enable_sleep
 *     Used to enable of disable sleep.
 *
 * @param[out] sl_status_t
 ******************************************************************************/
sl_status_t app_imu_sleep(bool enable_sleep)
{
  sl_status_t status;

  //Note: None EM2 capable EUSART instances needs special handling when entering
  //      or exiting EM2 otherwise it's not functional after EM2 wake-up.
  //      See em_eusart.h documentation for details

  if (!enable_sleep) {
    // Re-enable EUSART instance after waking from EM2 for proper operation
    // Only necessary for non EM2 capable instances
    EUSART_Enable(SL_ICM20689_SPI_EUSART_PERIPHERAL, eusartEnable);
  }

  status = sl_icm20689_enable_sleep_mode(enable_sleep);
  if (status == SL_STATUS_OK) {
    // Update flags
    if (enable_sleep) {
      imu_collecting = false;
      // Disable EUSART before entering EM2
      // Only necessary for non EM2 capable instances
      EUSART_Enable(SL_ICM20689_SPI_EUSART_PERIPHERAL, eusartDisable);
    } else {
      imu_collecting = true;
    }
  }

  return status;
}

/***************************************************************************//**
 * @brief
 *     Retrieves the orientation and acceleration vectors form the IMU.
 *     Orietation and acceleration vectors are pre-processed raw data from the
 *     IMU.
 *
 * @param[in] ovec
 *     Orinetation vector
 *
 * @param[in] avec
 *     Acceleration vector
 *
 * @param[out] status
 ******************************************************************************/
sl_status_t app_imu_get_data(int16_t ovec[3], int16_t avec[3])
{
  if (sl_imu_is_data_ready()) {
    sl_imu_update();
    sl_imu_get_orientation(ovec);
    sl_imu_get_acceleration(avec);
    samples_collected++;
    return SL_STATUS_OK;
  }

  return SL_STATUS_IN_PROGRESS;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     Utility function to get the IMU's ODR. A default value of 250.0 Hz is
 *     returned if no valid option is provided.
 ******************************************************************************/
static float get_imu_odr(imu_accel_gyro_odr_t odr)
{
  switch (odr) {
    case ACCEL_GYRO_ODR_3P9HZ:
      return 3.9;
      break;
    case ACCEL_GYRO_ODR_10P0HZ:
      return 10.0;
      break;
    case ACCEL_GYRO_ODR_15P4HZ:
      return 15.4;
      break;
    case ACCEL_GYRO_ODR_30P3HZ:
      return 30.3;
      break;
    case ACCEL_GYRO_ODR_50P0HZ:
      return 50.0;
      break;
    case ACCEL_GYRO_ODR_100P0HZ:
      return 100.0;
      break;
    case ACCEL_GYRO_ODR_125P0HZ:
      return 125.0;
      break;
    case ACCEL_GYRO_ODR_200P0HZ:
      return 200.0;
      break;
    case ACCEL_GYRO_ODR_250P0HZ:
      return 250.0;
      break;
    case ACCEL_GYRO_ODR_333P3HZ:
      return 333.3;
      break;
    case ACCEL_GYRO_ODR_500P0HZ:
      return 500.0;
      break;
    default:
      return 250.0;
      break;
  }
}

/***************************************************************************//**
 * @brief
 *     Utility function to verify if all the requested samples have been acquired
 ******************************************************************************/
static bool transfer_is_in_progress(void)
{
  // Check the status of current transfer
  if (samples_collected < IMU_SAMPLES_PER_CYCLE) {
    return true;
  } else {
    sample_buffer = imu_buffer;
    imu_collecting = false;
    return false;
  }
}

/***************************************************************************//**
 * @brief
 *     Utility function to copy buffer_len IMU samples to the provided
 *     buffer.
 ******************************************************************************/
static inline void copy_data_to_buffer(imu_6_axis_data_t* buffer_pnt,
                                       size_t buffer_len)
{
  memcpy(buffer_pnt, (int16_t*)sample_buffer, buffer_len);
}

/***************************************************************************//**
 * @brief
 *     Utility function used to configure the IMU GPIO interrupt pin.
 ******************************************************************************/
static void config_imu_int_gpio(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);

  // INT pin of IMU has active-low behavior
  GPIO_PinModeSet(SL_ICM20689_INT_PORT, SL_ICM20689_INT_PIN, gpioModeWiredAnd, 1);

  // Enable external interruption on falling edge
  GPIO_ExtIntConfig(SL_ICM20689_INT_PORT,
                    SL_ICM20689_INT_PIN,
                    SL_ICM20689_INT_PIN,
                    false,
                    true,
                    true);

  // Enable NVIC and respective IRQ handler
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
}
