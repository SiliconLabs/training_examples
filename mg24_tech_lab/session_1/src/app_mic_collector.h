/***************************************************************************//**
 * @file app_mic_collector.h
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

#ifndef APP_MIC_COLLECTOR_H_
#define APP_MIC_COLLECTOR_H_

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stddef.h>
#include <stdbool.h>
#include "sl_status.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define MIC_SAMPLES_PER_CYCLE       (115) // Number of samples required per request
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

sl_status_t app_mic_init(uint32_t sampling_frequency);
sl_status_t app_mic_enable(bool enable);
sl_status_t app_mic_process_action(int16_t* buffer_pnt);
sl_status_t app_mic_stop(void);
sl_status_t app_mic_start_stream(void);
sl_status_t app_mic_get_x_samples(uint32_t sample_cnt);

#ifdef __cplusplus
}
#endif

#endif /* APP_MIC_COLLECTOR_H_ */
