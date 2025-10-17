/**
 * @file
 * @brief RMT PPM frame transmitter definitions.
 * @internal
 *
 * @copyright (C) 2025 Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @endinternal
 *
 * @ingroup lib_ppm_bootloader
 *
 * @details Definitions of the RMT PPM frame transmitter module.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "driver/gpio.h"

#include "ppm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t tx_gpio_num;       /**< GPIO pin to use for TX */
    gpio_num_t rx_gpio_num;       /**< GPIO pin to use for RX */
} rmt_ppm_config_t;

/** Frame received callback type definition */
typedef void (*rmt_ppm_rx_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

/** Initialize and start component thread.
 *
 * @param[in]  cfg
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_init(const rmt_ppm_config_t *cfg);

/** De-initialize the RMT PPM module.
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_deinit(void);

/** Enable the RMT PPM module.
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_enable(void);

/** Disable the RMT PPM module.
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_disable(void);

/** Configure the average bitrate of the RMT PPM module.
 *
 * @param[in]  bitrate  bitrate to be applied from this calibration frame [bps].
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_set_bitrate(uint32_t bitrate);

/** Send the power on pattern on the bus.
 *
 * @param[in]  pattern_time  time to sent the pattern [us].
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_send_enter_ppm_pattern(uint32_t pattern_time);

/** Send the calibration frame on the bus.
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_send_calibration_frame(void);

/** Send a frame on the bus
 *
 * @param[in]  type     the frame type to be transmitted.
 * @param[in]  data     the data to be transmitted in this frame.
 * @param[in]  length   the length of the data to be transmitted in the frame (0..130 words).
 * @returns  error code representing the result of the action.
 */
esp_err_t rmt_ppm_send_frame(ppm_frame_type_t type, const uint16_t * data, size_t length);

/** Wait for some time to receive a valid ppm frame on the bus.
 *
 * @param[out]  type     the type of the received frame.
 * @param[out]  data     pointer to new object with the data of the received frame (object to be deleted by caller).
 * @param[in]   bus_timeout  time to wait for a response on the bus (in ms).
 * @return  the length of the data received.
 */
size_t rmt_ppm_wait_for_response_frame(ppm_frame_type_t * type, uint16_t ** data, uint16_t bus_timeout);

#ifdef __cplusplus
}
#endif
