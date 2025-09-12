/**
 * @file
 * @brief RMT PPM frame transmitter
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
 * @details Implementations of the RMT PPM frame transmitter.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "sdkconfig.h"

#include "rmt_ppm_encoder.h"
#include "sl_pwr_ctrl.h"

#include "rmt_ppm.h"

static const char *TAG = "rmt_ppm";

#define SYMBOLS_PER_BYTE 4

typedef union {
    uint8_t raw[1 + 256 + 2];
    struct __attribute__((packed)) {
        ppm_frame_type_t type;
        union {
            struct __attribute__((packed)) {
                uint8_t pulse_times[4];
                size_t pulse_len;
                uint32_t time;
            } epm_pattern;
            struct __attribute__((packed)) {} calib;
            struct __attribute__((packed)) {
                uint8_t data[256 + 2];
                size_t data_len;
            } frame;
        };
    };
} ppm_tx_item_t;

/** EPM pattern total length [us] */
const uint32_t epm_pattern_total = EPM_PATTERN_PULSE_TIME_1 + EPM_PATTERN_PULSE_TIME_2 +
                                   EPM_PATTERN_PULSE_TIME_3 + EPM_PATTERN_PULSE_TIME_4;

static rmt_encoder_handle_t ppm_encoder;

static rmt_channel_handle_t tx_chan = NULL;
static rmt_channel_handle_t rx_chan = NULL;

static gpio_num_t tx_gpio_num = GPIO_NUM_MAX;
static gpio_num_t rx_gpio_num = GPIO_NUM_MAX;

/** RMT module clock (0.25us units = 6digits diff) */
static uint32_t ppm_resolution_hz = 4000000u;
/** Minimum pulse time for current baudrate [ns] */
static uint32_t ppm_rx_min = 1000u;
/** Maximum pulse time for current baudrate [ns] */
static uint32_t ppm_rx_max = 22500u;

static size_t max_rx_data_len = 10;

static SemaphoreHandle_t tx_done_sem = NULL;

// Buffers for RMT symbols
static uint8_t rmt_symbols_buffer = 0u;
static rmt_symbol_word_t *rx_symbols[] = {NULL, NULL};
static size_t max_rx_symbols = 0;
static QueueHandle_t rx_queue = NULL;


static esp_err_t rmt_ppm_reconfigure_tx(uint32_t resolution_hz);
static esp_err_t rmt_ppm_reconfigure_rx(uint32_t resolution_hz);

/** RMT PPM decoder */
static esp_err_t ppm_decode_symbols(const rmt_symbol_word_t *symbols, size_t symbol_count, ppm_tx_item_t *item);

/** RMT TX done callback.
 *
 * @warning method is called in ISR context.
 */
static bool tx_done_cb(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx);

/** RMT RX done callback.
 *
 * @warning method is called in ISR context.
 */
static bool rx_done_cb(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx);


static esp_err_t rmt_ppm_reconfigure_tx(uint32_t resolution_hz) {
    if (tx_chan) {
        (void)rmt_disable(tx_chan);
        ESP_ERROR_CHECK(rmt_del_channel(tx_chan));
        tx_chan = NULL;
    }

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = tx_gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.with_dma = true,
        .flags.invert_out = true,
    };

    if (tx_gpio_num == rx_gpio_num) {
        tx_cfg.flags.io_od_mode = true;
    }

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &tx_chan);

    if (err == ESP_OK) {
        /* Register TX done callback */
        rmt_tx_event_callbacks_t tx_cbs = {
            .on_trans_done = tx_done_cb,
        };
        err = rmt_tx_register_event_callbacks(tx_chan, &tx_cbs, NULL);
    }

    if (err == ESP_OK) {
        err = rmt_enable(tx_chan);
    }

    return err;
}

static esp_err_t rmt_ppm_reconfigure_rx(uint32_t resolution_hz) {
    if (rx_chan) {
        (void)rmt_disable(rx_chan);
        ESP_ERROR_CHECK(rmt_del_channel(rx_chan));
        rx_chan = NULL;
    }

    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = rx_gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = 64,
        .flags.with_dma = true,
        .flags.invert_in = true,
    };
    esp_err_t err = rmt_new_rx_channel(&rx_cfg, &rx_chan);

    if (err == ESP_OK) {
        /* Register RX done callback */
        rmt_rx_event_callbacks_t rx_cbs = {
            .on_recv_done = rx_done_cb,
        };
        err = rmt_rx_register_event_callbacks(rx_chan, &rx_cbs, NULL);
    }

    if (err == ESP_OK) {
        err = rmt_enable(rx_chan);
    }

    return err;
}

static esp_err_t ppm_decode_symbols(const rmt_symbol_word_t *symbols, size_t symbol_count, ppm_tx_item_t *item) {
    size_t byte_idx = 0;
    uint8_t current_byte = 0;
    int bits_filled = 0;

    /* TODO allow for re-entering this function for partial reception */

    uint32_t frame_pulse = symbols[0].duration0 + symbols[0].duration1;
    if ((frame_pulse > (PPM_SESSION_PULSE_TIME - (PPM_BIT_DISTANCE / 2))) &&
        (frame_pulse < (PPM_SESSION_PULSE_TIME + (PPM_BIT_DISTANCE / 2)))) {
        item->type = ftSession;
    } else if ((frame_pulse > (PPM_PAGE_PULSE_TIME - (PPM_BIT_DISTANCE / 2))) &&
               (frame_pulse < (PPM_PAGE_PULSE_TIME + (PPM_BIT_DISTANCE / 2)))) {
        item->type = ftPage;
    } else {
        return ESP_FAIL;
    }

    for (size_t i = 1; i < symbol_count - 1; i++) {
        uint32_t total_time = symbols[i].duration0 + symbols[i].duration1;
        if ((total_time < (uint32_t)(4.5 * 4)) || (total_time > (uint32_t)(22.5 * 4))) {
            ESP_EARLY_LOGE(TAG, "Invalid symbol timing: %d us", (int)total_time);
            break;
        }

        uint8_t val = (uint8_t)((total_time - (uint32_t)(4.5 * 4)) / PPM_BIT_DISTANCE);

        current_byte = (current_byte << 2) | (val & 0x03);
        bits_filled += 2;

        if (bits_filled == 8) {
            if (byte_idx >= sizeof(item->frame.data)) {
                break;
            }
            item->frame.data[byte_idx++] = current_byte;
            current_byte = 0;
            bits_filled = 0;
        }
    }

    /* Handle partial last byte */
    if (bits_filled > 0 && byte_idx < sizeof(item->frame.data)) {
        current_byte <<= (8 - bits_filled);
        item->frame.data[byte_idx++] = current_byte;
    }
    item->frame.data_len = byte_idx;

    return ESP_OK;
}

static bool tx_done_cb(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(tx_done_sem, &xHigherPriorityTaskWoken);

    rmt_receive_config_t rx_cfg = {
        .signal_range_min_ns = ppm_rx_min,
        .signal_range_max_ns = ppm_rx_max,
        .flags = {
            .en_partial_rx = false,
        },
    };
    rmt_symbols_buffer ^= 1;
    esp_err_t err = rmt_receive(rx_chan,
                                rx_symbols[rmt_symbols_buffer],
                                max_rx_symbols * sizeof(rmt_symbol_word_t),
                                &rx_cfg);
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        /* this function is run in ISR context so no happy flow logging! */
        ESP_EARLY_LOGE(TAG, "RX start failed in TX done cb: %d", err);
    }

    return xHigherPriorityTaskWoken == pdTRUE;
}

static bool rx_done_cb(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    if (edata->flags.is_last) {
        /* this was an rx done based on timeout event so re-enable receiver */
        rmt_receive_config_t rx_cfg = {
            .signal_range_min_ns = ppm_rx_min,
            .signal_range_max_ns = ppm_rx_max,
            .flags = {
                .en_partial_rx = false,
#if 0
                TODO put timeout to max, decode chunks in buffer and enqueue full frames
                do we really need this disable https ://github.com/espressif/esp-idf/blob/346870a3044010f2018be0ef3b86ba650251c655/components/esp_driver_rmt/src/rmt_rx.c#L815
#endif
            },
        };
        rmt_symbols_buffer ^= 1;
        esp_err_t err = rmt_receive(rx_chan,
                                    rx_symbols[rmt_symbols_buffer],
                                    max_rx_symbols * sizeof(rmt_symbol_word_t),
                                    &rx_cfg);
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
            /* this function is run in ISR context so no happy flow logging! */
            ESP_EARLY_LOGE(TAG, "RX start failed in RX done cb: %d", err);
        }
    } else {
        /* this was an rx done based on partial rx event */
    }

    size_t num_symbols = edata->num_symbols;
    if (num_symbols > max_rx_symbols) {
        num_symbols = max_rx_symbols;
    }

    ppm_tx_item_t item;
    if (ppm_decode_symbols(edata->received_symbols, num_symbols, &item) == ESP_OK) {
        if (xQueueSend(rx_queue, &item, 0) != pdTRUE) {
            ESP_EARLY_LOGE(TAG, "RX queue full");
            return ESP_ERR_NO_MEM;
        }
    }

    return false; // no context switch needed
}

esp_err_t rmt_ppm_init(const rmt_ppm_config_t *cfg) {
    if ((!cfg) || (cfg->tx_gpio_num == GPIO_NUM_MAX) || (cfg->rx_gpio_num == GPIO_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Init PPM component on GPIO %d %d", cfg->tx_gpio_num, cfg->rx_gpio_num);

    tx_gpio_num = cfg->tx_gpio_num;
    rx_gpio_num = cfg->rx_gpio_num;

    ESP_ERROR_CHECK(rmt_ppm_reconfigure_tx(ppm_resolution_hz));
    ESP_ERROR_CHECK(rmt_ppm_reconfigure_rx(ppm_resolution_hz));

    max_rx_symbols = max_rx_data_len * SYMBOLS_PER_BYTE;
    rmt_symbols_buffer = 0;
    rx_symbols[0] = calloc(max_rx_symbols, sizeof(rmt_symbol_word_t));
    rx_symbols[1] = calloc(max_rx_symbols, sizeof(rmt_symbol_word_t));
    if (!rx_symbols[0] || !rx_symbols[1]) {
        free(rx_symbols[0]);
        free(rx_symbols[1]);
        ESP_LOGE(TAG, "Failed to allocate symbol buffers");
        return ESP_ERR_NO_MEM;
    }

    tx_done_sem = xSemaphoreCreateBinary();
    if (!tx_done_sem) {
        ESP_LOGE(TAG, "Failed to create TX done semaphore");
        return ESP_ERR_NO_MEM;
    }

    rmt_ppm_encoder_config_t rmt_ppm_enc_cfg = {};
    ESP_ERROR_CHECK(rmt_ppm_encoder_new(&rmt_ppm_enc_cfg, &ppm_encoder));

    rx_queue = xQueueCreate(4, sizeof(ppm_tx_item_t));
    if (!rx_queue) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t rmt_ppm_deinit(void) {
    if (tx_done_sem) {
        vSemaphoreDelete(tx_done_sem);
        tx_done_sem = NULL;
    }

    rmt_ppm_encoder_delete(ppm_encoder);
    ppm_encoder = NULL;

    free(rx_symbols[0]);
    rx_symbols[0] = NULL;
    free(rx_symbols[1]);
    rx_symbols[1] = NULL;

    if (tx_chan) {
        (void)rmt_disable(tx_chan);
        (void)rmt_del_channel(tx_chan);
        tx_chan = NULL;
    }

    if (rx_chan) {
        (void)rmt_disable(rx_chan);
        (void)rmt_del_channel(rx_chan);
        rx_chan = NULL;
    }

    if (rx_queue) {
        vQueueDelete(rx_queue);
        rx_queue = NULL;
    }

    return ESP_OK;
}

esp_err_t rmt_ppm_set_bitrate(uint32_t bitrate) {
    if (bitrate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The ppm protocol bitrate is changing based on the number of 1 and 0's to transfer.
     * Assuming we transfer as much 1's as we transfer 0's one could estimate
     * (for the default baud/timings) an average pulse time as:
     * - 0b00 = 4.5us = 18 * 0.25us
     * - 0b01 = 6.0us = 24 * 0.25us
     * - 0b10 = 7.5us = 30 * 0.25us
     * - 0b11 = 9.0us = 36 * 0.25us
     * - average pulse = 6,75us = 27 * 0.25us
     * As the protocol transfers 2 bits per pulse:
     *   avg_baud = 2 / 6,75us = 296296 bps = 4000000 * 2 / 27
     *
     * Min pulse = 1us
     * Max pulse = 22.5us
     */
    ppm_resolution_hz = bitrate / 2u * 27u;
    ppm_rx_min = 8000000000u / 27u / bitrate;
    ppm_rx_max = 20000000000u / 3u / bitrate;

    return ESP_OK;
}

esp_err_t rmt_ppm_send_enter_ppm_pattern(uint32_t pattern_time) {
    if (pattern_time == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)rmt_disable(rx_chan);
    esp_err_t err = rmt_enable(rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enable RX failed: %d", err);
        return ESP_FAIL;
    }

    ppm_tx_item_t item = { .type = ftEnter_Ppm };
    item.epm_pattern.pulse_times[0] = EPM_PATTERN_PULSE_TIME_1;
    item.epm_pattern.pulse_times[1] = EPM_PATTERN_PULSE_TIME_2;
    item.epm_pattern.pulse_times[2] = EPM_PATTERN_PULSE_TIME_3;
    item.epm_pattern.pulse_times[3] = EPM_PATTERN_PULSE_TIME_4;
    item.epm_pattern.pulse_len = 4;
    item.epm_pattern.time = pattern_time;

    uint32_t loop_count = item.epm_pattern.time / epm_pattern_total;
    if (loop_count == 0) {
        loop_count = 1;
    }

    rmt_transmit_config_t tx_cfg = {.loop_count = loop_count};
    err = rmt_transmit(tx_chan, ppm_encoder, item.raw, item.epm_pattern.pulse_len, &tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX failed: %d", err);
        return ESP_FAIL;
    }

    slpwrctrl_enable();

    /* Wait for TX done via callback semaphore */
    if (xSemaphoreTake(tx_done_sem, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "TX done wait failed");
    }

    return ESP_OK;
}

esp_err_t rmt_ppm_send_calibration_frame(void) {
    (void)rmt_disable(rx_chan);
    esp_err_t err = rmt_enable(rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enable RX failed: %d", err);
        return ESP_FAIL;
    }

    ppm_tx_item_t item = { .type = ftCalibration };
    rmt_transmit_config_t tx_cfg = {.loop_count = 0};
    err = rmt_transmit(tx_chan, ppm_encoder, item.raw, 1, &tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX failed: %d", err);
        return ESP_FAIL;
    }

    /* Wait for TX done via callback semaphore */
    if (xSemaphoreTake(tx_done_sem, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "TX done wait failed");
    }

    return ESP_OK;
}

esp_err_t rmt_ppm_send_frame(ppm_frame_type_t type, const uint16_t * data, size_t length) {
    if (!data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)rmt_disable(rx_chan);
    esp_err_t err = rmt_enable(rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enable RX failed: %d", err);
        return ESP_FAIL;
    }

    ppm_tx_item_t item;
    memset(&item, 0, sizeof(item));
    item.type = type;
    for (size_t i = 0; i < length; i++) {
        item.frame.data[i * 2] = (uint8_t)(data[i] >> 8);
        item.frame.data[(i * 2) + 1] = (uint8_t)(data[i] >> 0);
    }
    item.frame.data_len = length * 2;

    rmt_transmit_config_t tx_cfg = {.loop_count = 0};
    err = rmt_transmit(tx_chan, ppm_encoder, item.raw, item.frame.data_len, &tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX failed: %d", err);
        return ESP_FAIL;
    }

    /* Wait for TX done via callback semaphore */
    if (xSemaphoreTake(tx_done_sem, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "TX done wait failed");
    }

    return ESP_OK;
}

size_t rmt_ppm_wait_for_response_frame(ppm_frame_type_t * type, uint16_t ** data, uint16_t bus_timeout) {
    if (!type || !data) {
        return 0;
    }

    size_t retval = 0;
    ppm_tx_item_t item;
    if (xQueueReceive(rx_queue, &item, bus_timeout / portTICK_PERIOD_MS) == pdTRUE) {
        *type = item.type;
        retval = item.frame.data_len / 2;
        uint16_t * buffer = calloc(retval, sizeof(uint16_t));
        for (size_t i = 0; i < retval; i++) {
            buffer[i] = ((uint16_t)item.frame.data[i * 2]) << 8;
            buffer[i] |= ((uint16_t)item.frame.data[(i * 2) + 1]) << 0;
        }
        *data = buffer;
    }

    return retval;
}
