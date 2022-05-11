/***************************************************************************//**
 * @file app.c
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "app.h"

#include "app_mic_collector.h"
#include "app_imu_collector.h"
#include "app_rht_collector.h"

#include "sl_sleeptimer.h"
#include "sl_power_manager.h"
#include "sl_board_control.h"
#include "printf.h"

#include "em_gpio.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

/// Samples to capture per cycle for the different sensors
#define RHT_SENSOR_SAMPLES  RHT_SAMPLES_PER_CYCLE
#define MICROPHONE_SAMPLES  MIC_SAMPLES_PER_CYCLE
#define IMU_SENSOR_SAMPLES  IMU_SAMPLES_PER_CYCLE

/// Sensor sampling frequency configuration symbols
#define IMU_SAMPLING_FREQUENCY_HZ (ACCEL_GYRO_ODR_500P0HZ)
#define MIC_SAMPLING_FREQUENCY_HZ (16000)

/// Sensor enabling configuration symbols
#define ENABLE_RHT_SENSOR   (1)
#define ENABLE_MICROPHONE   (1)
#define ENABLE_IMU_SENSOR   (1)

/// Sensor data capture cycle latency symbol
#define SENSOR_MEASUREMENT_DELAY_MS 1000 // Sensor measurement cycle time

/// Print related symbols
#define DEBUG_PRINTS_ENABLED (1)

#if DEBUG_PRINTS_ENABLED
#define MIC_SAMPLE_PRINT     (1)
#define IMU_SAMPLE_PRINT     (1)
#define RHT_SAMPLE_PRINT     (1)
#endif

#if DEBUG_PRINTS_ENABLED

#define debug_printf(...) printf(__VA_ARGS__)
#define log_printf(...) printf(__VA_ARGS__)

#else

#define debug_printf(...)
#define log_printf(...)

#endif

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static void verify_sensors_data_ready(void);
static void dispatch_sensor_data(void);
static void schedule_sensor_data_collection(void);

static void sensor_measurement_delay(uint16_t time_ms,
                                     volatile bool *control_flag);
static void timer_expiration_callback(sl_sleeptimer_timer_handle_t *handle,
                                      void *data);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
volatile bool start_new_cycle = true;

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static sl_sleeptimer_timer_handle_t delay_timer; //Sleep timer instance

#if ENABLE_MICROPHONE
static bool mic_data_scheduled = false; // Mic data capture scheduling flag
static int16_t microphone_sample_buffer[MICROPHONE_SAMPLES] = {0x0};
#endif
static bool mic_data_ready = false;     // Mic data ready flag

#if ENABLE_IMU_SENSOR
static bool imu_data_scheduled = false; // IMU data capture scheduling flag
static imu_6_axis_data_t imu_sample_buffer[IMU_SENSOR_SAMPLES] = {0x0};
#endif
static bool imu_data_ready = false;     // IMU data ready flag
static volatile bool imu_sample_ready = false;

#if ENABLE_RHT_SENSOR
static bool rht_data_scheduled = false; // RHT data capture scheduling flag
static rht_sensor_data_t rht_sample_buffer[RHT_SAMPLES_PER_CYCLE] = {0x0};
#endif
static bool rht_data_ready = false;     // RHT data ready flag

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  sl_status_t status = SL_STATUS_OK;

  // Enable voltage scaling to reduce power in EM2/EM3. It increase wakeup time
  // ~50.7uS form EM2
  sl_power_manager_em23_voltage_scaling_enable_fast_wakeup(false);

#if ENABLE_RHT_SENSOR
  sl_board_enable_sensor(SL_BOARD_SENSOR_RHT); // Power on the RHT sensor

  status = app_rht_init();
  if (status != SL_STATUS_OK) {
    debug_printf("Error 0x%lx during RHT sensor init!", status);
    while(1);
  }
#else
  // Needs to be explicitly disabled because I2CSPM is enabled in autogen code
  app_rht_deinit();
  // The RHT is powered from the same signal as the other I2C sensors and IMU in
  // BRD2601B therefore, be careful when managing the SL_BOARD_SENSOR_RHT signal
  //sl_board_disable_sensor(SL_BOARD_SENSOR_RHT);
#endif

#if ENABLE_MICROPHONE
  status = app_mic_init(MIC_SAMPLING_FREQUENCY_HZ); // Initialize the microphone collector
  if (status != SL_STATUS_OK) {
    debug_printf("Error 0x%lx during microphone init!", status);
    while(1);
  }
#else
  // Ensure that MIC_ENABLE signal is set low (Unpower microphones)
  sl_board_disable_sensor(SL_BOARD_SENSOR_MICROPHONE);
#endif

#if ENABLE_IMU_SENSOR
  sl_board_enable_sensor(SL_BOARD_SENSOR_IMU); // Power on the IMU sensor

  status = app_imu_init(IMU_SAMPLING_FREQUENCY_HZ);
  if (status != SL_STATUS_OK) {
    debug_printf("Error 0x%lx during IMU init!", status);
    while(1);
  }
#else
  // The IMU is powered from the same signal as the I2C sensors in BRD2601B
  // therefore, be careful with managing the SL_BOARD_SENSOR_IMU signal
  //sl_board_disable_sensor(SL_BOARD_SENSOR_IMU);
#endif
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  verify_sensors_data_ready();

  // New sensor measurement cycle, repeats every SENSOR_MEASUREMENT_DELAY_MS
  if (start_new_cycle) {

    // Schedule next cycle
    sensor_measurement_delay(SENSOR_MEASUREMENT_DELAY_MS, &start_new_cycle);

    dispatch_sensor_data(); // Used to handle/process sensor data (e.g: print)
    schedule_sensor_data_collection(); // Request sensors to collect samples
  }
}

/***************************************************************************//**
 * GPIO ODD IRQ handler. Used to detect if an IMU sample  is ready.
 ******************************************************************************/
void GPIO_ODD_IRQHandler(void)
{
  // Get and clear all pending GPIO interrupts
  uint32_t interruptMask = GPIO_IntGet();
  GPIO_IntClear(interruptMask);

  // Check if SL_ICM20689_INT_PIN is the source
  if ( interruptMask & (1 << SL_ICM20689_INT_PIN) ) {
    imu_sample_ready = true;
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/***************************************************************************//**
 * @brief
 *     If the sensor data collection has been scheduled, it retrieves the data
 *     ONLY if the whole sample set has been collected.
 ******************************************************************************/
static void verify_sensors_data_ready(void)
{
  sl_status_t status = SL_STATUS_OK;

  // Check if microphone data is ready.
  //   Mic will wake the system when all LDMA transfers have finished
#if ENABLE_MICROPHONE
  if (mic_data_scheduled) {
    status = app_mic_process_action(microphone_sample_buffer);
    if (status == SL_STATUS_IDLE) {
      // We can schedule mic capture in the next cycle
      mic_data_scheduled = false;
    } else if (status == SL_STATUS_OK) {
      app_mic_enable(false);
      // Data is ready remove power manager requirement an allow lower energy mode
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
      mic_data_scheduled = false;
      mic_data_ready = true;
    }
  }
#endif

  // Check if IMU data is ready.
  //   IMU will wake the system through a GPIO IRQ each time a new conversion
  //   is ready
#if ENABLE_IMU_SENSOR
  if (imu_data_scheduled && imu_sample_ready) {
    imu_sample_ready = false;
    status = app_imu_process_action(imu_sample_buffer);
    if (status == SL_STATUS_IDLE) {
      // The IMU was previously unpowered so we need to configure it again
      imu_data_scheduled = false;
    } else if (status == SL_STATUS_OK) {
      app_imu_sleep(true); // Send IMU to sleep
      // Data is ready, remove power manager requirement, disable IMU and allow lower energy mode
      sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
      imu_data_scheduled = false;
      imu_data_ready = true;
    }
  }
#endif

  // Check if RHT sensor data is ready.
  //   The RHT works in a polled manner but we could expand it's control for
  //   example using a periodic sleeptimer
#if ENABLE_RHT_SENSOR
  if (rht_data_scheduled) {
    status = app_rht_process_action(rht_sample_buffer);
    if (status == SL_STATUS_IDLE) {
      rht_data_scheduled = false;
    } else if (status == SL_STATUS_OK) {
      // Data is ready
      rht_data_scheduled = false;
      rht_data_ready = true;
    }
  }
#endif
}

/***************************************************************************//**
 * @brief
 *     Utility function used to process the collected data, in this case it's
 *     only printed if required. Updates status flags
 ******************************************************************************/
static void dispatch_sensor_data(void)
{
  // Dispatch sensor data
  if ( rht_data_ready || mic_data_ready || imu_data_ready) {
#if RHT_SAMPLE_PRINT && ENABLE_RHT_SENSOR
    log_printf("\r\nRHT:\r\n");
    for (uint8_t i = 0; i < RHT_SENSOR_SAMPLES; i++) {
      log_printf("%d,%d, ", rht_sample_buffer[i].relative_humidity,
                        rht_sample_buffer[i].temperature);
    }
#endif
#if MIC_SAMPLE_PRINT && ENABLE_MICROPHONE
    log_printf("\r\nMic:\r\n");
    for (uint16_t i = 0; i < MICROPHONE_SAMPLES; i++) {
      log_printf("%d, ", microphone_sample_buffer[i]);
    }
#endif
#if IMU_SAMPLE_PRINT && ENABLE_IMU_SENSOR
    log_printf("\r\nIMU:\r\n");
    for (uint16_t i = 0; i < IMU_SENSOR_SAMPLES; i++) {
      log_printf("%d,%d,%d_", imu_sample_buffer[i].orientation[0],
                              imu_sample_buffer[i].orientation[1],
                              imu_sample_buffer[i].orientation[2]);
      log_printf("%d,%d,%d ", imu_sample_buffer[i].acceleration[0],
                              imu_sample_buffer[i].acceleration[1],
                              imu_sample_buffer[i].acceleration[2]);
    }
    log_printf("\n");
#endif

    // Data was handled, clear flags
    mic_data_ready = false;
    imu_data_ready = false;
    rht_data_ready = false;
  }
}

/***************************************************************************//**
 * @brief
 *     Utility function used to schedule the sensor data collection.
 ******************************************************************************/
static void schedule_sensor_data_collection(void)
{
  sl_status_t status;

#if ENABLE_MICROPHONE
  if (!mic_data_scheduled && !mic_data_ready) {
    app_mic_enable(true);
    status = app_mic_get_x_samples(MICROPHONE_SAMPLES);
    if (status != SL_STATUS_OK) {
      debug_printf("Error 0x%lx requesting mic sensor data!", status);
    }

    mic_data_scheduled = true;
    mic_data_ready = false;

    // Restrict energy mode to EM1 while we capture microphone data
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
#endif

#if ENABLE_IMU_SENSOR
  if (!imu_data_scheduled && !imu_data_ready) {
    status = app_imu_sleep(false); //Wake up the IMU
    if (status != SL_STATUS_OK) {
      debug_printf("Error 0x%lx waking IMU sensor!", status);
    }

    imu_data_scheduled = true;
    imu_data_ready = false;

    // Restrict energy mode to EM1 while we capture IMU data
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
#endif

#if ENABLE_RHT_SENSOR
  if (!rht_data_scheduled && !rht_data_ready) {

    rht_data_scheduled = true;
    rht_data_ready = false;
  }
#endif
}

/***************************************************************************//**
 * @brief
 *     Initiates a timer used to space the sensor measurements. The system is
 *     allowed to sleep in the meantime.
 *
 * @param[in] time_ms
 *     Time in milliseconds between measurements.
 *
 * @param[in] control_flag
 *     Control flag to indicate if a measurement delay is ongoing.
 ******************************************************************************/
static void sensor_measurement_delay(uint16_t time_ms,
                                     volatile bool *control_flag)
{
  sl_status_t status;

  *control_flag = false; // Set callback variable to false

  uint32_t delay = sl_sleeptimer_ms_to_tick(time_ms);

  status = sl_sleeptimer_start_timer(&delay_timer,
                                     delay,
                                     timer_expiration_callback,
                                     (void *)control_flag,
                                     0,
                                     0);

  if (status != SL_STATUS_OK) {
    while (1); // This point shouldn't be reached
  }
}

/*******************************************************************************
 * Timer expiration callback.
 *
 * @param[in] handle
 *     Pointer to sleeptimer handle.
 *
 * @param[in] data
 *     Pointer to callback data.
 ******************************************************************************/
static void timer_expiration_callback(sl_sleeptimer_timer_handle_t *handle,
                                      void *data)
{

  volatile bool *wait_flag = (bool *)data;

  (void)&handle; // Unused parameter.

  *wait_flag = true;
}
