/**
 * @file
 * @brief Ppm sessions
 * @internal
 *
 * @copyright (C) 2018-2025 Melexis N.V.
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
 * @details Implementation of the ppm sessions.
 */
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mlx_crc.h"
#include "rmt_ppm.h"

#include "ppm_types.h"
#include "ppm_session.h"

static const char *TAG = "ppm_session";

/** Send a session frame on the bus
 *
 * @param[in]  config  session configuration.
 * @param[in]  page_count  number of pages to be transmitted in this session.
 * @param[in]  offset  offset to be used by this programming session.
 * @param[in]  checksum  checksum for the to be programmed memory.
 *
 * @return  an error code representing the result of the operation.
 */
static esp_err_t send_session_frame(const ppm_session_config_t * config,
                                    uint16_t page_count,
                                    uint16_t offset,
                                    uint16_t checksum);

/** Receive a session acknowledge from the bus
 *
 * @param[out]  rx_data  the acknowledge frame data received (to be deleted by consumer).
 * @param[in]  timeout  the timeout to wait for an acknowledge to be received (in ms).
 *
 * @return  the length of the received data.
 */
static uint16_t receive_session_ack(uint16_t ** rx_data, uint16_t bus_timeout);

/** Send a page frame on the bus
 *
 * @param[in]  sequence_number  number of pages to be transmitted in this session.
 * @param[in]  page_checksum  offset to be used by this programming session.
 * @param[in]  data_words  checksum for the to be programmed memory.
 * @param[in]  data_length  length of the data to transmit.
 *
 * @return  an error code representing the result of the operation.
 */
static esp_err_t send_page_frame(uint8_t sequence_number,
                                 uint8_t page_checksum,
                                 const uint16_t * data_words,
                                 size_t data_length);

/** Receive a page acknowledge from the bus
 *
 * @param[out]  rx_data  the acknowledge frame data received (to be deleted by consumer).
 * @param[in]  timeout  the timeout to wait for an acknowledge to be received.
 *
 * @return  the length of the received data.
 */
static uint16_t receive_page_ack(uint16_t ** rx_data, uint16_t bus_timeout);

/** Handle a complete session
 *
 * This method will handle a complete ppm session, meaning it will:
 * - send session frame
 * - send page frame(s)
 * - read/verify page ack(s) -> if enabled
 * - read/verify session ack -> if enabled
 *
 * @param[in]  config  session configuration.
 * @param[in]  offset  offset to be used by this programming session.
 * @param[in]  checksum  checksum for the to be programmed memory.
 * @param[in]  page_data  page data to be send to the ppm slave.
 * @param[in]  page_data_len  length of the page data to be send (needs to be page aligned).
 * @param[in]  rx_data  data which was received in the session acknowledge.
 *
 * @return  length of the response data.
 */
static size_t handle_session(const ppm_session_config_t * config,
                             uint16_t offset,
                             uint16_t checksum,
                             const uint16_t * page_data,
                             uint32_t page_data_len,
                             uint16_t ** rx_data);


static esp_err_t send_session_frame(const ppm_session_config_t * config,
                                    uint16_t page_count,
                                    uint16_t offset,
                                    uint16_t checksum) {
    /* assemble the command byte */
    uint8_t session_command = config->session_id;

    if (config->request_ack != false) {
        session_command |= 0x80u;
    }

    /* create the frame data */
    uint16_t session_frame[4];
    session_frame[0] = (((uint16_t)session_command) << 8) | ((uint16_t)config->page_size);
    session_frame[1] = page_count;
    session_frame[2] = offset;
    session_frame[3] = checksum;

    /* send the frame and wait for the response (first response is the TX message to verify) */
    return rmt_ppm_send_frame(ftSession, session_frame, 4u);
}

static uint16_t receive_session_ack(uint16_t ** rx_data, uint16_t bus_timeout) {
    uint16_t rx_lenght = 0u;

    if (rx_data != NULL) {
        ppm_frame_type_t type = ftUnknown;
        rx_lenght = rmt_ppm_wait_for_response_frame(&type, rx_data, bus_timeout);

        if (*rx_data != NULL) {
            /* apply "https://jira.melexis.com/jira/browse/MLX81332-77" workaround */
            (*rx_data)[0] -= 1u;
        }

        if (type != ftSession) {
            /* not expected acknowledge session type received */
            rx_lenght = 0u;

            if (*rx_data != NULL) {
                free(*rx_data);
                *rx_data = NULL;
            }
        }
    } else {
        /* no pointer provided, this makes no sense */
        ESP_LOGE(TAG, "rx_data pointer is empty");
    }

    return rx_lenght;
}

static esp_err_t send_page_frame(uint8_t sequence_number,
                                 uint8_t page_checksum,
                                 const uint16_t * data_words,
                                 size_t data_length) {
    esp_err_t result = ESP_FAIL;

    if ((data_length <= 128u) && (data_words != NULL)) {
        /* lets start creating the frame data */
        uint16_t * page_frame = (uint16_t*)calloc(1u + data_length, sizeof(uint16_t));
        if (page_frame != NULL) {
            page_frame[0] = (((uint16_t)sequence_number) << 8) | ((uint16_t)page_checksum);
            memcpy(&page_frame[1], &data_words[0], data_length);

            /* send the frame and wait for the response (first response is the TX message to verify) */
            result = rmt_ppm_send_frame(ftPage, page_frame, 1u + data_length);
        } else {
            /* mem allocation failed */
            ESP_LOGE(TAG, "mem allocation failed for send page frame");
            return ESP_ERR_NO_MEM;
        }
        free(page_frame);
    } else {
        /* incorrect data length or no buffer */
        ESP_LOGE(TAG, "incorrect data length of incorrect pointer received");
        return ESP_ERR_INVALID_ARG;
    }

    return result;
}

static uint16_t receive_page_ack(uint16_t ** rx_data, uint16_t bus_timeout) {
    uint16_t rx_lenght = 0u;

    if (rx_data != NULL) {
        ppm_frame_type_t type = ftUnknown;
        rx_lenght = rmt_ppm_wait_for_response_frame(&type, rx_data, bus_timeout);

        if (type != ftPage) {
            /* not expected acknowledge session type received */
            rx_lenght = 0u;

            if (*rx_data != NULL) {
                free(*rx_data);
                *rx_data = NULL;
            }
        }
    } else {
        /* incorrect buffer */
        ESP_LOGE(TAG, "incorrect pointer received");
    }

    return rx_lenght;
}

static size_t handle_session(const ppm_session_config_t * config,
                             uint16_t offset,
                             uint16_t checksum,
                             const uint16_t * page_data,
                             uint32_t page_data_len,
                             uint16_t ** rx_data) {
    size_t ret_len = 0;
    uint16_t page_count = 0u;
    uint16_t session_ack_timeout = config->session_ack_timeout;

    if (config->page_size != 0u) {
        page_count = ceil((float)page_data_len / config->page_size);
    }

    if (send_session_frame(config, page_count, offset, checksum) == ESP_OK) {
        bool blPageSuccess = true;

        if ((page_data != NULL) && (page_count != 0u)) {
            /* handle all page frames */
            uint16_t * page_data_words = (uint16_t*)calloc(config->page_size, sizeof(uint16_t));
            if (page_data_words != NULL) {
                for (uint16_t seqnr = 0u; seqnr < page_count; seqnr++) {
                    /* get the relevant data words for this page frame */
                    memcpy(&page_data_words[0], &page_data[seqnr * config->page_size], config->page_size);
                    uint16_t page_checksum = crc_calcPageChecksum(page_data_words, config->page_size);

                    blPageSuccess = false;

                    for (uint16_t retry = 0u; retry < config->page_retry; retry++) {
                        if (send_page_frame(seqnr & 0xFFu, page_checksum, page_data_words,
                                            config->page_size) == ESP_OK) {
                            uint16_t page_frame_timeout;

                            if (seqnr == 0u) {
                                page_frame_timeout = config->page0_ack_timeout;
                            } else {
                                page_frame_timeout = config->pageX_ack_timeout;
                            }

                            if (config->request_ack == false) {
                                /* wait for fixed time for write/erase to be done */
                                vTaskDelay(page_frame_timeout / portTICK_PERIOD_MS);

                                blPageSuccess = true;
                                break;
                            } else {
                                /* wait for page ack */
                                uint16_t * resp_data = NULL;
                                uint16_t resp_len = receive_page_ack(&resp_data, page_frame_timeout);

                                if ((resp_data != NULL) && (resp_len > 0u)) {
                                    if (resp_data[0] == (((seqnr & 0xFFu) << 8) | (page_checksum & 0xFFu))) {
                                        blPageSuccess = true;
                                    }
                                }

                                free(resp_data);

                                if (blPageSuccess == true) {
                                    break;
                                }
                            }
                        }
                    }

                    if (blPageSuccess == false) {
                        ESP_LOGE(TAG, "page programming failed after retries");
                        break;
                    }
                }
            } else {
                /* mem allocation failed */
                ESP_LOGE(TAG, "mem allocation failed for handle session");
            }
            free(page_data_words);
        } else {
            /* no data needs transmission */
        }

        if (blPageSuccess == true) {
            if (config->request_ack == false) {
                /* wait for session to be done */
                vTaskDelay(session_ack_timeout / portTICK_PERIOD_MS);
            } else {
                /* wait for session ack */
                if (rx_data != NULL) {
                    uint16_t resp_len = receive_session_ack(rx_data, session_ack_timeout);

                    if (*rx_data != NULL) {
                        uint16_t session_command = (uint16_t)config->session_id;

                        if ( ((*rx_data)[0] == ((session_command << 8) | ((uint16_t)config->page_size))) &&
                             ((*rx_data)[1] == page_count) ) {
                            ret_len = resp_len;
                        } else {
                            free(*rx_data);
                            *rx_data = NULL;
                        }
                    } else {
                        /* no session ack was received */
                        ESP_LOGE(TAG, "no session ack received");
                    }
                }
            }
        }
    }

    return ret_len;
}

esp_err_t ppmsession_doUnlock(const ppm_session_config_t * config, uint16_t * project_id) {
    esp_err_t result = ESP_FAIL;
    uint16_t * rx_data = NULL;

    ESP_LOGD(TAG, "do unlock session");

    size_t rx_length = handle_session(config, 0x8374u, 0xBF12u, NULL, 0u, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if (rx_length == 4) {
            *project_id = rx_data[3];
            result = ESP_OK;
        } else {
            ESP_LOGE(TAG, "incorrect unlock session response length");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not */
            ESP_LOGE(TAG, "no unlock session response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doFlashProgKeys(const ppm_session_config_t * config, const uint16_t * prog_keys, size_t length) {
    esp_err_t result = ESP_FAIL;
    uint16_t * rx_data = NULL;

    ESP_LOGD(TAG, "do flash prog keys session");

    size_t rx_length = handle_session(config, 0xBEBEu, 0xBEBEu, prog_keys, length, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if ((rx_length == 4) && (rx_data[2] == 0xBEBEu) && (rx_data[3] == 0xBEBEu)) {
            result = ESP_OK;
        } else {
            ESP_LOGE(TAG, "incorrect flash prog keys response");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not*/
            ESP_LOGE(TAG, "no flash prog keys response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doFlashProgramming(const ppm_session_config_t * config,
                                        const uint8_t * flash_bytes,
                                        size_t length) {
    esp_err_t result = ESP_FAIL;
    uint16_t words_length;
    uint16_t * flash_words = NULL;

    ESP_LOGD(TAG, "do flash programming session");

    words_length = ceil((double)length / 2);
    flash_words = (uint16_t*)calloc(words_length + config->page_size, sizeof(uint16_t));

    if (flash_words != NULL) {
        uint16_t * rx_data = NULL;

        for (uint16_t ctr = 0u; ctr < words_length; ctr++) {
            flash_words[ctr] = (uint16_t)(flash_bytes[ctr * 2]) | ((uint16_t)(flash_bytes[(ctr * 2) + 1]) << 8);
        }

        uint32_t flash_crc = config->crc_func(flash_words, words_length, 1u);

        /* we need to start at page 1 and end with page 0!!!! */
        for (uint16_t ctr = 0u; ctr < config->page_size; ctr++) {
            flash_words[words_length + ctr] = flash_words[ctr];
        }

        size_t rx_length = handle_session(config,
                                          (uint16_t)((flash_crc >> 16) & 0xFFu),
                                          (uint16_t)flash_crc,
                                          &flash_words[config->page_size],
                                          words_length,
                                          &rx_data);

        if (rx_data != NULL) {
            /* lets check the ack content */
            if ((rx_length == 4) &&
                (rx_data[2] == (uint16_t)((flash_crc >> 16) & 0xFFu)) &&
                (rx_data[3] == (uint16_t)flash_crc)) {
                result = ESP_OK;
            } else {
                ESP_LOGE(TAG, "incorrect flash programming response");
            }
        } else {
            /* no ack was received */
            if (config->request_ack == true) {
                /* we should have gotten an ack... but we did not*/
                ESP_LOGE(TAG, "no flash programming response received");
            } else {
                /* no response expected at all */
                result = ESP_OK;
            }
        }

        free(rx_data);
    } else {
        /* mem allocation failed */
        ESP_LOGE(TAG, "mem allocation failed for flash programming do session");
    }

    free(flash_words);

    return result;
}

esp_err_t ppmsession_doEepromProgramming(const ppm_session_config_t * config,
                                         uint16_t mem_offset,
                                         const uint8_t * data_bytes,
                                         size_t data_length) {
    esp_err_t result = ESP_FAIL;
    uint16_t words_length = ceil((double)data_length / 2);
    uint16_t page_offset = ceil((double)mem_offset / 2 / config->page_size);

    ESP_LOGD(TAG, "do eeprom programming session");

    uint16_t eeprom_crc = crc_calc16bitCrc(data_bytes, data_length, 0x1D0Fu);

    uint16_t * rx_data = NULL;
    size_t rx_length = handle_session(config,
                                      page_offset,
                                      eeprom_crc,
                                      (uint16_t *)(&data_bytes[0]),
                                      words_length,
                                      &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        ESP_LOGE(TAG, "crc calc = %d  chip = %d", eeprom_crc, rx_data[3]);

        if ((rx_length == 4) && (rx_data[3] == eeprom_crc)) {
            result = ESP_OK;
        } else {
            ESP_LOGE(TAG, "incorrect eeprom programming response");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not*/
            ESP_LOGE(TAG, "no eeprom programming response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doFlashCsProgramming(const ppm_session_config_t * config,
                                          const uint8_t * data_bytes,
                                          size_t data_length) {
    esp_err_t result = ESP_FAIL;
    uint16_t words_length = ceil((double)data_length / 2);

    ESP_LOGD(TAG, "do flash cs programming session");

    uint16_t flash_crc = crc_calc16bitCrc(data_bytes, data_length, 0x1D0Fu);

    uint16_t * rx_data = NULL;
    size_t rx_length = handle_session(config,
                                      0u,                            // offset
                                      flash_crc,                     // checksum
                                      (uint16_t *)(&data_bytes[0]),  // page_data
                                      words_length,                  // page_data_len
                                      &rx_data);                     // rx_data


    if (rx_data != NULL) {
        /* lets check the ack content */
        if ((rx_length == 4) && (rx_data[2] == 0u) && (rx_data[3] == flash_crc)) {
            result = ESP_OK;
        } else {
            ESP_LOGE(TAG, "incorrect flash cs programming response");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not*/
            ESP_LOGE(TAG, "no flash cs programming response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doFlashCrc(const ppm_session_config_t * config, size_t length, uint32_t * crc) {
    esp_err_t result = ESP_FAIL;
    uint16_t * rx_data = NULL;
    uint16_t words_length = ceil((double)length / 2);

    ESP_LOGD(TAG, "do ppm flash crc session");

    size_t rx_length = handle_session(config, 0x0u, 0x0u, NULL, words_length, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if (rx_length == 4) {
            if (crc != NULL) {
                *crc = ((uint32_t)(rx_data[2] & 0xFFu) << 16) | (uint32_t)(rx_data[3]);
                result = ESP_OK;
            } else {
                ESP_LOGE(TAG, "invalid crc pointer received");
            }
        } else {
            ESP_LOGE(TAG, "incorrect flash crc response length");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not */
            ESP_LOGE(TAG, "no flash crc session response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doEepromCrc(const ppm_session_config_t * config,
                                 uint16_t offset,
                                 size_t length,
                                 uint16_t * crc) {
    esp_err_t result = ESP_FAIL;
    uint16_t words_length = ceil((double)length / 2);
    uint16_t page_offset = ceil((double)offset / 2 / config->page_size);

    ESP_LOGD(TAG, "do ppm eeprom crc session");

    uint16_t * rx_data = NULL;
    size_t rx_length = handle_session(config, page_offset, 0x0u, NULL, words_length, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if (rx_length == 4) {
            if (crc != NULL) {
                *crc = rx_data[3];
                result = ESP_OK;
            } else {
                ESP_LOGE(TAG, "invalid crc pointer received");
            }
        } else {
            ESP_LOGE(TAG, "incorrect eeprom crc response length");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not */
            ESP_LOGE(TAG, "no eeprom crc session response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doFlashCsCrc(const ppm_session_config_t * config, size_t length, uint16_t * crc) {
    esp_err_t result = ESP_FAIL;
    uint16_t words_length = ceil((double)length / 2);

    ESP_LOGD(TAG, "do ppm Flash CS crc session");

    uint16_t * rx_data = NULL;
    size_t rx_length = handle_session(config, 0x0u, 0x0u, NULL, words_length, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if (rx_length == 4) {
            if (crc != NULL) {
                *crc = rx_data[3];
                result = ESP_OK;
            } else {
                ESP_LOGE(TAG, "invalid crc pointer received");
            }
        } else {
            ESP_LOGE(TAG, "incorrect Flash CS crc response length");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not */
            ESP_LOGE(TAG, "no Flash CS crc session response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

esp_err_t ppmsession_doChipReset(const ppm_session_config_t * config, uint16_t * project_id) {
    esp_err_t result = ESP_FAIL;
    uint16_t * rx_data = NULL;

    ESP_LOGD(TAG, "do chip reset session");

    size_t rx_length = handle_session(config, 0x0u, 0x0u, NULL, 0u, &rx_data);

    if (rx_data != NULL) {
        /* lets check the ack content */
        if (rx_length == 4) {
            *project_id = rx_data[3];
            result = ESP_OK;
        } else {
            ESP_LOGE(TAG, "incorrect chip reset session response length");
        }
    } else {
        /* no ack was received */
        if (config->request_ack == true) {
            /* we should have gotten an ack... but we did not */
            ESP_LOGE(TAG, "no chip reset session response received");
        } else {
            /* no response expected at all */
            result = ESP_OK;
        }
    }

    free(rx_data);

    return result;
}

