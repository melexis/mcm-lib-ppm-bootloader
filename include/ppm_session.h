/**
 * @file
 * @brief Ppm session definitions.
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
 * @details Definitions of the ppm session module.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mlx_crc.h"
#include "ppm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Unlock session mode PPM session default configuration */
#define PPM_SESSION_UNLOCK_DEFAULT { \
            .session_id = PPM_SESSION_UNLOCK, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 0u, \
            .pageX_ack_timeout = 0u, \
            .session_ack_timeout = 10u, \
            .crc_func = NULL, \
}

/** Programming keys PPM session default configuration */
#define PPM_SESSION_PROG_KEYS_DEFAULT { \
            .session_id = PPM_SESSION_PROG_KEYS, \
            .page_size = 8u, \
            .request_ack = true, \
            .page_retry = 1u, \
            .page0_ack_timeout = 25u, \
            .pageX_ack_timeout = 10u, \
            .session_ack_timeout = 10u, \
            .crc_func = NULL, \
}

/** Amalthea flash programming PPM session default configuration */
#define PPM_SESSION_FLASH_PROG_AMALTHEA_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_PROG, \
            .page_size = 64u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 100u, \
            .pageX_ack_timeout = 10u, \
            .session_ack_timeout = 10u, \
            .crc_func = crc_calc24bitCrc, \
}

/** Ganymede XFE flash programming PPM session default configuration */
#define PPM_SESSION_FLASH_PROG_GANY_XFE_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_PROG, \
            .page_size = 64u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 100u, \
            .pageX_ack_timeout = 10u, \
            .session_ack_timeout = 10u, \
            .crc_func = crc_calcGanyXfeCrc, \
}

/** Ganymede KF flash programming PPM session default configuration */
#define PPM_SESSION_FLASH_PROG_GANY_KF_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_PROG, \
            .page_size = 64u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 100u, \
            .pageX_ack_timeout = 10u, \
            .session_ack_timeout = 10u, \
            .crc_func = crc_calcGanyKfCrc, \
}

/** EEPROM programming PPM session default configuration */
#define PPM_SESSION_EEPROM_PROG_DEFAULT { \
            .session_id = PPM_SESSION_EEPROM_PROG, \
            .page_size = 4u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 15u, \
            .pageX_ack_timeout = 15u, \
            .session_ack_timeout = 17u, \
            .crc_func = NULL, \
}

/** IUM programming PPM session default configuration */
#define PPM_SESSION_IUM_PROG_DEFAULT { \
            .session_id = PPM_SESSION_EEPROM_PROG, \
            .page_size = 64u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 8u, \
            .pageX_ack_timeout = 8u, \
            .session_ack_timeout = 10u, \
            .crc_func = NULL, \
}

/** Flash CS programming PPM session default configuration */
#define PPM_SESSION_FLASH_CS_PROG_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_CS_PROG, \
            .page_size = 64u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 50u, \
            .pageX_ack_timeout = 7u, \
            .session_ack_timeout = 15u, \
            .crc_func = NULL, \
}

/** Flash CRC PPM session default configuration */
#define PPM_SESSION_FLASH_CRC_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_CRC, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 0u, \
            .pageX_ack_timeout = 0u, \
            .session_ack_timeout = 5u, \
            .crc_func = NULL, \
}

/** EEPROM CRC PPM session default configuration */
#define PPM_SESSION_EEPROM_CRC_DEFAULT { \
            .session_id = PPM_SESSION_EEPROM_CRC, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 0u, \
            .pageX_ack_timeout = 0u, \
            .session_ack_timeout = 5u, \
            .crc_func = NULL, \
}

/** IUM CRC PPM session default configuration */
#define PPM_SESSION_IUM_CRC_DEFAULT { \
            .session_id = PPM_SESSION_EEPROM_CRC, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .page0_ack_timeout = 0u, \
            .pageX_ack_timeout = 0u, \
            .session_ack_timeout = 8u, \
            .crc_func = NULL, \
}

/** Flash CS CRC PPM session default configuration */
#define PPM_SESSION_FLASH_CS_CRC_DEFAULT { \
            .session_id = PPM_SESSION_FLASH_CS_CRC, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .pageX_ack_timeout = 0u, \
            .page0_ack_timeout = 0u, \
            .session_ack_timeout = 5u, \
            .crc_func = NULL, \
}

/** Reset PPM session default configuration */
#define PPM_SESSION_CHIP_RESET_DEFAULT { \
            .session_id = PPM_SESSION_CHIP_RESET, \
            .page_size = 0u, \
            .request_ack = true, \
            .page_retry = 5u, \
            .pageX_ack_timeout = 0u, \
            .page0_ack_timeout = 0u, \
            .session_ack_timeout = 10u, \
            .crc_func = NULL, \
}

/** Send unlock session mode on the bus
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5/specification/ppm_bootloader.markdown#L194
 *
 * @param[in]  config  session configuration.
 * @param[out]  project_id  in case acknowledge request is enabled this will get the project ID of the slave.
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doUnlock(const ppm_session_config_t * config, uint16_t * project_id);

/** Send flash programming keys session on the bus
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5/specification/ppm_bootloader.markdown#L219
 *
 * @param[in]  config  session configuration.
 * @param[in]  prog_keys  programming keys to transfer.
 * @param[in]  length  length of the flash programming keys to upload.
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doFlashProgKeys(const ppm_session_config_t * config,
                                     const uint16_t * prog_keys,
                                     size_t length);

/** Send a amalthea flash programming session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5/specification/ppm_bootloader.markdown#L261
 *
 * @param[in]  config  session configuration.
 * @param[in]  flash_bytes  flash to upload (hex file content).
 * @param[in]  length  length of the flash to upload.
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doFlashProgramming(const ppm_session_config_t * config,
                                        const uint8_t * flash_bytes,
                                        size_t length);

/** Send an eeprom programming session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5.2/specification/ppm_bootloader.markdown#eeprom-programming-session
 *
 * @param[in]  config  session configuration.
 * @param[in]  mem_offset  offset in the eeprom to start programming from (in bytes).
 * @param[in]  data_bytes  eeprom data to upload (needs to be of size page_size*n).
 * @param[in]  data_length  length of the eeprom data to upload (in bytes).
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doEepromProgramming(const ppm_session_config_t * config,
                                         uint16_t mem_offset,
                                         const uint8_t * data_bytes,
                                         size_t data_length);

/** Send a flash cs programming session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/-/blob/1.5/specification/ppm_bootloader.markdown#L296
 *
 * @param[in]  config  session configuration.
 * @param[in]  data_bytes  flash to upload (hex file content).
 * @param[in]  data_length  length of the flash cs to upload.
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doFlashCsProgramming(const ppm_session_config_t * config,
                                          const uint8_t * data_bytes,
                                          size_t data_length);

/** Send a flash crc session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5.2/specification/ppm_bootloader.markdown#flash-crc-read-session
 *
 * @param[in]  config  session configuration.
 * @param[in]  length  number of bytes to calculate the crc for (calculation starts from flash address 0)(will be page aligned).
 * @param[out]  crc  chip calculated flash crc value (24-bit).
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doFlashCrc(const ppm_session_config_t * config, size_t length, uint32_t * crc);

/** Send an eeprom crc session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/-/blob/1.7.1/specification/ppm_bootloader.markdown#eeprom-crc-read-session
 *
 * @param[in]  config  session configuration.
 * @param[in]  offset  number of bytes from the start of the memory to start calculating from (shall be page aligned).
 * @param[in]  length  number of bytes to calculate the crc for (shall be page aligned).
 * @param[out]  crc  chip calculated eeprom crc value (16-bit).
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doEepromCrc(const ppm_session_config_t * config,
                                 uint16_t offset,
                                 size_t length,
                                 uint16_t * crc);

/** Send an Flash CS crc session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/-/blob/1.8.0/specification/ppm_bootloader.markdown#flash-cs-crc-read-session
 *
 * @param[in]  config  session configuration.
 * @param[in]  length  number of bytes to calculate the crc for (shall be page aligned).
 * @param[out]  crc  chip calculated flash crc value (16-bit).
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doFlashCsCrc(const ppm_session_config_t * config, size_t length, uint16_t * crc);

/** Send a chip reset session
 *
 * Send frame as defined in:
 * https://gitlab.melexis.com/bu-act/docu/ppm_bootloader/blob/1.5/specification/ppm_bootloader.markdown#L300
 *
 * @param[in]  config  session configuration.
 * @param[out]  project_id  in case acknowledge request is enabled this will get the project ID of the slave.
 *
 * @return  an error code representing the result of the operation.
 */
esp_err_t ppmsession_doChipReset(const ppm_session_config_t * config, uint16_t * project_id);

#ifdef __cplusplus
}
#endif
