/**
 * @file
 * @brief PPM types definitions.
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
 * @details Definitions of the PPM types module.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mlx_crc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PPM distance between 2 pulse types [1/4 us] */
#define PPM_BIT_DISTANCE (1.5 * 4)

/** PPM pulse low time [1/4 us] */
#define PPM_PULSE_LOW_TIME (1.5 * 4)

/** PPM session pulse time [1/4us] */
#define PPM_SESSION_PULSE_TIME (12 * 4)

/** PPM page pulse time [1/4us] */
#define PPM_PAGE_PULSE_TIME (13.5 * 4)

/** PPM calibration pulse time [1/4us] */
#define PPM_CALIB_PULSE_TIME (18.75 * 4)

/** EPM pattern pulse 1 length [us] */
#define EPM_PATTERN_PULSE_TIME_1 30

/** EPM pattern pulse 2 length [us] */
#define EPM_PATTERN_PULSE_TIME_2 90

/** EPM pattern pulse 3 length [us] */
#define EPM_PATTERN_PULSE_TIME_3 45

/** EPM pattern pulse 4 length [us] */
#define EPM_PATTERN_PULSE_TIME_4 45

/** ppm frame type enum */
typedef enum __attribute__((packed)) ppm_frame_type_e {
    ftSession = 0,                      /**< session frame type */
    ftPage = 1,                         /**< page frame type */
    ftCalibration = 2,                  /**< calibration frame type */
    ftEnter_Ppm = 3,                    /**< enter ppm pattern frame type */
    ftUnknown = 0xFF,                   /**< unknown frame type */
} ppm_frame_type_t;                     /**< ppm frame type */

/** ppm session id enum */
typedef enum session_id_e {
    PPM_SESSION_PROG_KEYS = 0x03u,      /**< programming keys session id */
    PPM_SESSION_FLASH_PROG = 0x04u,     /**< flash programming session id */
    PPM_SESSION_EEPROM_PROG = 0x06u,    /**< eeprom programming session id */
    PPM_SESSION_FLASH_CS_PROG = 0x07u,  /**< flash cs programming session id */
    PPM_SESSION_RAM_PROG = 0x08u,       /**< ram program programming session id */
    PPM_SESSION_FLASH_CRC = 0x43u,      /**< flash crc session id */
    PPM_SESSION_UNLOCK = 0x44u,         /**< unlock session mode session id */
    PPM_SESSION_CHIP_RESET = 0x45u,     /**< chip reset session id */
    PPM_SESSION_EEPROM_CRC = 0x47u,     /**< eeprom crc session id */
    PPM_SESSION_FLASH_CS_CRC = 0x48u,   /**< flash cs crc session id */
} session_id_t;                         /**< ppm session id type */

/** ppm session configuration structure */
typedef struct ppm_session_s {
    session_id_t session_id;            /**< session type identifier (0x00..0x7F) */
    uint8_t page_size;                  /**< page size (in words) of this session's pages (0x00..0xFF) */
    bool request_ack;                   /**< request an acknowledge from the slave (default enabled) */
    uint8_t page_retry;                 /**< number of page retries which are allowed */
    uint16_t pageX_ack_timeout;         /**< page acknowledge timeout (ms) */
    uint16_t page0_ack_timeout;         /**< first page acknowledge timeout (ms) */
    uint16_t session_ack_timeout;       /**< session acknowledge timeout (ms) */
    flash_crc_func_t crc_func;          /**< memory crc calculation method */
} ppm_session_config_t;                 /**< ppm session configuration type */

#ifdef __cplusplus
}
#endif
