/***************************************************************************//**
 * @file app_mic_collector.h
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

#ifndef APP_IMU_COLLECTOR_H_
#define APP_IMU_COLLECTOR_H_

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stddef.h>
#include "sl_status.h"
#include "sl_imu.h"
#include "sl_icm20689_config.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define IMU_SAMPLES_PER_CYCLE       (125) // Number of samples required per request
#define IMU_SENSOR_AXIS_COUNT       (3) //3-axis per sensor

// Data type for IMU acceleration and orientation vector data
typedef struct imu_6_axis_data {
  int16_t acceleration[IMU_SENSOR_AXIS_COUNT];
  int16_t orientation[IMU_SENSOR_AXIS_COUNT];
} imu_6_axis_data_t;

/***************************************************************************//**
 * Potential IMU reported status when issuing sl_imu_get_state()
 ******************************************************************************/
typedef enum app_imu_status {
  IMU_DISABLED     = IMU_STATE_DISABLED,
  IMU_READY        = IMU_STATE_READY,
  IMU_INITIALIZING = IMU_STATE_INITIALIZING,
  IMU_CALIBRATING  = IMU_STATE_CALIBRATING
} app_imu_status_t;

/***************************************************************************//**
 * ICM-20689 output data rates (ODR) for 6-axis mode, based on datasheet.
 * Normal mode is the default configuration of the sl_imu driver.
 * This is only a subset of values, other rates may be achievable based on
 * configuration.
 ******************************************************************************/
typedef enum imu_accel_gyro_odr {
  ACCEL_GYRO_ODR_3P9HZ   = 0,
  ACCEL_GYRO_ODR_10P0HZ  = 1,
  ACCEL_GYRO_ODR_15P4HZ  = 2,
  ACCEL_GYRO_ODR_30P3HZ  = 3,
  ACCEL_GYRO_ODR_50P0HZ  = 4,
  ACCEL_GYRO_ODR_100P0HZ = 5,
  ACCEL_GYRO_ODR_125P0HZ = 6,
  ACCEL_GYRO_ODR_200P0HZ = 7,
  ACCEL_GYRO_ODR_250P0HZ = 8,
  ACCEL_GYRO_ODR_333P3HZ = 9,
  ACCEL_GYRO_ODR_500P0HZ = 10
} imu_accel_gyro_odr_t;

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

sl_status_t app_imu_init(imu_accel_gyro_odr_t sample_rate);
sl_status_t app_imu_process_action(imu_6_axis_data_t *buf_pnt);
sl_status_t app_imu_stop(void);
sl_status_t app_imu_get_data(int16_t ovec[3], int16_t avec[3]);
sl_status_t app_imu_sleep(bool enable_sleep);

#ifdef __cplusplus
}
#endif

#endif /* APP_IMU_COLLECTOR_H_ */
