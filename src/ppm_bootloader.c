/**
 * @file
 * @brief PPM bootloader module.
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
 * @details Implementations of the PPM bootloader module.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "intelhex.h"
#include "mlx_chip.h"
#include "mlx_crc.h"

#include "ppm_err.h"
#include "ppm_session.h"
#include "rmt_ppm.h"

#include "ppm_bootloader.h"


/** Request the ic to enter into programming mode
 *
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @param[in]  bitrate  bitrate to be used [bps].
 * @param[in]  pattern_time  time to transmit enter ppm mode pattern (in ms).
 * @param[out]  chip_info  information about the connected chip.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_enterProgrammingMode(bool broadcast,
                                             uint32_t bitrate,
                                             uint32_t pattern_time,
                                             const MlxChip_t ** chip_info);

/** Request the ic to exit from programming mode
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_exitProgrammingMode(const MlxChip_t * chip_info, bool broadcast);

/** Program the flash memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @param[in]  ihex  intel hex container to be programmed.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_programFlashMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex);

/** Verify the flash memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  ihex  intel hex container to be verified.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_verifyFlashMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex);

/** Program the flash cs memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @param[in]  ihex  intel hex container to be programmed.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_programFlashCsMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex);

/** Verify the flash cs memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  ihex  intel hex container to be verified.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_verifyFlashCsMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex);

/** Program the eeprom memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @param[in]  ihex  intel hex container to be programmed.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_programEepromMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex);

/** Verify the eeprom memory of the connected ic
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  ihex  intel hex container to be verified.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_verifyEepromMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex);

/** Check and if needed execute a programming keys session
 *
 * @param[in]  chip_info  information about the connected chip as returned by enter_programming_mode().
 * @param[in]  broadcast  en/disable broadcast mode during upload.
 * @return  an error code representing the result of the operation.
 */
static ppm_err_t ppmbtl_checkAndDoProgKeysSession(const MlxChip_t * chip_info, bool broadcast);


static ppm_err_t ppmbtl_enterProgrammingMode(bool broadcast,
                                             uint32_t bitrate,
                                             uint32_t pattern_time,
                                             const MlxChip_t ** chip_info) {
    ppm_err_t result = PPM_OK;

    if (chip_info != NULL) {
        if (rmt_ppm_send_enter_ppm_pattern(pattern_time) != ESP_OK) {
            result = PPM_FAIL_BTL_ENTER_PPM_MODE;
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

        if (result == PPM_OK) {
            if (rmt_ppm_set_bitrate(bitrate) != ESP_OK) {
                result = PPM_FAIL_SET_BAUD;
            }
        }

        if (result == PPM_OK) {
            if (rmt_ppm_send_calibration_frame() != ESP_OK) {
                result = PPM_FAIL_CALIBRATION;
            }
        }

        uint16_t project_id;
        if (result == PPM_OK) {
            ppm_session_config_t unlock_cfg = PPM_SESSION_UNLOCK_DEFAULT;
            unlock_cfg.request_ack = !broadcast;
            if (ppmsession_doUnlock(&unlock_cfg, &project_id) != ESP_OK) {
                result = PPM_FAIL_UNLOCK;
            }
        }

        if (result == PPM_OK) {
            *chip_info = mlxchip_getCamcuChip(project_id);
            if ((*chip_info != NULL) && ((*chip_info)->bootloaders.ppm_loader == NULL)) {
                result = PPM_FAIL_CHIP_NOT_SUPPORTED;
            }
        }
    } else {
        result = PPM_FAIL_INTERNAL;
    }

    return result;
}

static ppm_err_t ppmbtl_exitProgrammingMode(const MlxChip_t * chip_info, bool broadcast) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if (chip_info != NULL) {
        uint16_t proj_id_resp;
        ppm_session_config_t reset_cfg = PPM_SESSION_CHIP_RESET_DEFAULT;
        reset_cfg.request_ack = !broadcast;
        if (ppmsession_doChipReset(&reset_cfg, &proj_id_resp) == ESP_OK) {
            result = PPM_OK;
        }
    } else {
        result = PPM_FAIL_INTERNAL;
    }
    return result;
}

static ppm_err_t ppmbtl_programFlashMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex) {
    ppm_err_t result;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        result = ppmbtl_checkAndDoProgKeysSession(chip_info, broadcast);
        if (result == PPM_OK) {
            uint32_t memStart = chip_info->memories.flash->start;
            uint32_t memEnd = chip_info->memories.flash->start + chip_info->memories.flash->length - 1;

            if ((intelhex_minAddress(ihex) > memEnd) || (intelhex_maxAddress(ihex) < memStart)) {
                result = PPM_FAIL_MISSING_DATA;
            } else {
                size_t memLen = chip_info->memories.flash->length;
                uint8_t *content = malloc(memLen);
                if (content == NULL) {
                    result = PPM_FAIL_INTERNAL;
                } else {
                    (void)intelhex_getFilled(ihex, memStart, content, memLen);

                    ppm_session_config_t session_cfg = PPM_SESSION_FLASH_PROG_AMALTHEA_DEFAULT;
                    session_cfg.request_ack = !broadcast;
                    session_cfg.page_size = chip_info->memories.flash->page / sizeof(uint16_t);
                    session_cfg.page0_ack_timeout = (uint16_t)(memLen / chip_info->memories.flash->erase_unit *
                                                               chip_info->memories.flash->erase_time * 1.25);
                    session_cfg.pageX_ack_timeout = (uint16_t)(chip_info->memories.flash->write_time * 1.25);
                    session_cfg.session_ack_timeout = session_cfg.pageX_ack_timeout + (uint16_t)(memLen * 0.0000625);
                    if (ppmsession_doFlashProgramming(&session_cfg, &content[0], memLen) != PPM_OK) {
                        result = PPM_FAIL_PROGRAMMING_FAILED;
                    }
                }
                free(content);
            }
        }
    }
    return result;
}

static ppm_err_t ppmbtl_verifyFlashMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        size_t memLen = chip_info->memories.flash->length;
        uint32_t memStart = chip_info->memories.flash->start;
        uint32_t memEnd = chip_info->memories.flash->start + chip_info->memories.flash->length - 1;

        if ((memLen <= 4) ||
            (intelhex_minAddress(ihex) > memEnd) ||
            (intelhex_maxAddress(ihex) < memStart)) {
            result = PPM_FAIL_MISSING_DATA;
        } else {
            uint16_t *content = (uint16_t *)malloc(memLen);
            if (content == NULL) {
                result = PPM_FAIL_INTERNAL;
            } else {
                (void)intelhex_getFilled(ihex, memStart, (uint8_t*)content, memLen);
                uint32_t hex_crc = crc_calc24bitCrc(content, memLen / 2, 1u);
                uint32_t chip_crc;

                ppm_session_config_t session_cfg = PPM_SESSION_FLASH_CRC_DEFAULT;
                session_cfg.page_size = chip_info->memories.flash->page / sizeof(uint16_t);
                session_cfg.session_ack_timeout = (uint16_t)(memLen * 0.0000625);
                if ((ppmsession_doFlashCrc(&session_cfg, memLen, &chip_crc) != ESP_OK) || (chip_crc != hex_crc)) {
                    result = PPM_FAIL_VERIFY_FAILED;
                } else {
                    result = PPM_OK;
                }
            }
            free(content);
        }
    }
    return result;
}

static ppm_err_t ppmbtl_programFlashCsMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        result = ppmbtl_checkAndDoProgKeysSession(chip_info, broadcast);
        if (result == PPM_OK) {
            uint32_t memStart = chip_info->memories.flash_cs->start;
            uint32_t memEnd = chip_info->memories.flash_cs->start + chip_info->memories.flash_cs->writeable - 1;

            if ((intelhex_minAddress(ihex) > memEnd) || (intelhex_maxAddress(ihex) < memStart)) {
                result = PPM_FAIL_MISSING_DATA;
            } else {
                /* determine length to program */
                size_t memLen = intelhex_maxAddress(ihex) - chip_info->memories.flash_cs->start + 1;
                if (memLen > chip_info->memories.flash_cs->writeable) {
                    memLen = chip_info->memories.flash_cs->writeable;
                }
                /* make page aligned */
                if ((memLen % chip_info->memories.flash_cs->page) != 0) {
                    memLen = memLen - (memLen % chip_info->memories.flash_cs->page) +
                             chip_info->memories.flash_cs->page;
                }
                uint8_t *content = malloc(memLen);
                if (content == NULL) {
                    result = PPM_FAIL_INTERNAL;
                } else {
                    (void)intelhex_getFilled(ihex, memStart, content, memLen);

                    ppm_session_config_t session_cfg = PPM_SESSION_FLASH_CS_PROG_DEFAULT;
                    session_cfg.request_ack = !broadcast;
                    session_cfg.page_size = chip_info->memories.flash_cs->page / sizeof(uint16_t);
                    session_cfg.page0_ack_timeout = (uint16_t)(memLen / chip_info->memories.flash_cs->page *
                                                               chip_info->memories.flash_cs->erase_time * 1.25);
                    session_cfg.pageX_ack_timeout = (uint16_t)(chip_info->memories.flash_cs->write_time * 1.25);
                    session_cfg.session_ack_timeout = session_cfg.pageX_ack_timeout + (uint16_t)(memLen * 0.0000625);
                    if (ppmsession_doFlashCsProgramming(&session_cfg, &content[0], memLen) != PPM_OK) {
                        result = PPM_FAIL_PROGRAMMING_FAILED;
                    }
                }
                free(content);
            }
        }
    }
    return result;
}

static ppm_err_t ppmbtl_verifyFlashCsMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        uint32_t memStart = chip_info->memories.flash_cs->start;
        uint32_t memEnd = chip_info->memories.flash_cs->start + chip_info->memories.flash_cs->length - 1;

        if ((intelhex_minAddress(ihex) > memEnd) || (intelhex_maxAddress(ihex) < memStart)) {
            result = PPM_FAIL_MISSING_DATA;
        } else {
            /* determine length to program */
            size_t memLen = intelhex_maxAddress(ihex) - chip_info->memories.flash_cs->start + 1;
            if (memLen > chip_info->memories.flash_cs->length) {
                memLen = chip_info->memories.flash_cs->length;
            }
            /* make page aligned */
            if ((memLen % chip_info->memories.flash_cs->page) != 0) {
                memLen = memLen - (memLen % chip_info->memories.flash_cs->page) +
                         chip_info->memories.flash_cs->page;
            }
            uint8_t *content = malloc(memLen);
            if (content == NULL) {
                result = PPM_FAIL_INTERNAL;
            } else {
                (void)intelhex_getFilled(ihex, memStart, (uint8_t*)content, memLen);

                uint16_t hex_crc = crc_calc16bitCrc(&content[0], memLen, 0x1D0Fu);
                uint16_t chip_crc;
                ppm_session_config_t session_cfg = PPM_SESSION_FLASH_CS_CRC_DEFAULT;
                session_cfg.page_size = chip_info->memories.flash_cs->page / sizeof(uint16_t);
                if ((ppmsession_doFlashCsCrc(&session_cfg, memLen, &chip_crc) != ESP_OK) || (chip_crc != hex_crc)) {
                    result = PPM_FAIL_VERIFY_FAILED;
                } else {
                    result = PPM_OK;
                }
            }
            free(content);
        }
    }
    return result;
}

static ppm_err_t ppmbtl_programEepromMemory(const MlxChip_t * chip_info, bool broadcast, ihexContainer_t * ihex) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        result = ppmbtl_checkAndDoProgKeysSession(chip_info, broadcast);
        if (result == PPM_OK) {
            uint32_t memStart = chip_info->memories.nv_memory->start;
            uint32_t memEnd = chip_info->memories.nv_memory->start + chip_info->memories.nv_memory->writeable - 1;

            if ((intelhex_minAddress(ihex) > memEnd) || (intelhex_maxAddress(ihex) < memStart)) {
                result = PPM_FAIL_MISSING_DATA;
            } else {
                uint8_t *content = malloc(chip_info->memories.nv_memory->writeable);
                if (content == NULL) {
                    result = PPM_FAIL_INTERNAL;
                } else {
                    /* assemble blocks of eeprom pages */
                    uint32_t currAddr = memStart;
                    while ((currAddr < memEnd) && (result == PPM_OK)) {
                        bool inBlock = true;
                        uint16_t currLen = 0u;
                        uint16_t currOff = currAddr - memStart;
                        while ((inBlock) && (currAddr < memEnd)) {
                            if (intelhex_countBytesInRange(ihex,
                                                           currAddr,
                                                           chip_info->memories.nv_memory->page) != 0) {
                                (void)intelhex_getFilled(ihex,
                                                         currAddr,
                                                         &content[currLen],
                                                         chip_info->memories.nv_memory->page);
                                currLen += chip_info->memories.nv_memory->page;
                                currAddr += chip_info->memories.nv_memory->page;
                            } else {
                                inBlock = false;
                                currAddr = currAddr + chip_info->memories.nv_memory->page;
                            }
                        }
                        if (currLen > 0) {
                            /* perform eeprom prog session */
                            ppm_session_config_t session_cfg = PPM_SESSION_EEPROM_PROG_DEFAULT;
                            session_cfg.request_ack = !broadcast;
                            session_cfg.page_size = chip_info->memories.nv_memory->page / sizeof(uint16_t);
                            session_cfg.page0_ack_timeout = (uint16_t)(chip_info->memories.nv_memory->write_time *
                                                                       1.25);
                            session_cfg.pageX_ack_timeout = (uint16_t)(chip_info->memories.nv_memory->write_time *
                                                                       1.25);
                            session_cfg.session_ack_timeout = session_cfg.pageX_ack_timeout;
                            if (ppmsession_doEepromProgramming(&session_cfg,
                                                               currOff,
                                                               &content[0],
                                                               currLen) != PPM_OK) {
                                result = PPM_FAIL_PROGRAMMING_FAILED;
                            }
                        }
                    }
                }
                free(content);
            }
        }
    }
    return result;
}

static ppm_err_t ppmbtl_verifyEepromMemory(const MlxChip_t * chip_info, ihexContainer_t * ihex) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;
    if ((ihex == NULL) || (chip_info == NULL)) {
        result = PPM_FAIL_INTERNAL;
    } else {
        uint32_t memStart = chip_info->memories.nv_memory->start;
        uint32_t memEnd = chip_info->memories.nv_memory->start + chip_info->memories.nv_memory->length - 1;

        if ((intelhex_minAddress(ihex) > memEnd) || (intelhex_maxAddress(ihex) < memStart)) {
            result = PPM_FAIL_MISSING_DATA;
        } else {
            uint8_t *content = malloc(chip_info->memories.nv_memory->writeable);
            if (content == NULL) {
                result = PPM_FAIL_INTERNAL;
            } else {
                /* assemble blocks of eeprom pages */
                uint32_t currAddr = memStart;
                result = PPM_OK;
                while ((currAddr < memEnd) && (result == PPM_OK)) {
                    bool inBlock = true;
                    uint16_t currLen = 0u;
                    uint16_t currOff = currAddr - memStart;
                    while ((inBlock) && (currAddr < memEnd)) {
                        if (intelhex_countBytesInRange(ihex,
                                                       currAddr,
                                                       chip_info->memories.nv_memory->page) != 0) {
                            (void)intelhex_getFilled(ihex,
                                                     currAddr,
                                                     &content[currLen],
                                                     chip_info->memories.nv_memory->page);
                            currLen += chip_info->memories.nv_memory->page;
                            currAddr += chip_info->memories.nv_memory->page;
                        } else {
                            inBlock = false;
                            currAddr = currAddr + chip_info->memories.nv_memory->page;
                        }
                    }
                    if (currLen > 0) {
                        /* perform eeprom crc session */
                        uint16_t hex_crc = crc_calc16bitCrc(&content[0], currLen, 0x1D0Fu);
                        uint16_t chip_crc;
                        ppm_session_config_t session_cfg = PPM_SESSION_EEPROM_CRC_DEFAULT;
                        session_cfg.page_size = chip_info->memories.nv_memory->page / sizeof(uint16_t);
                        if ((ppmsession_doEepromCrc(&session_cfg, currOff, currLen, &chip_crc) != ESP_OK) ||
                            (chip_crc != hex_crc)) {
                            result = PPM_FAIL_VERIFY_FAILED;
                        }
                    }
                }
            }
            free(content);
        }
    }
    return result;
}

static ppm_err_t ppmbtl_checkAndDoProgKeysSession(const MlxChip_t * chip_info, bool broadcast) {
    ppm_err_t result = PPM_FAIL_UNKNOWN;

    if ((chip_info->bootloaders.ppm_loader != NULL) &&
        (chip_info->bootloaders.ppm_loader->prog_keys != NULL)) {
        ppm_session_config_t prog_keys_cfg = PPM_SESSION_PROG_KEYS_DEFAULT;
        prog_keys_cfg.request_ack = !broadcast;
        if (ppmsession_doFlashProgKeys(&prog_keys_cfg,
                                       chip_info->bootloaders.ppm_loader->prog_keys->values,
                                       chip_info->bootloaders.ppm_loader->prog_keys->length) == ESP_OK) {
            result = PPM_OK;
        }
    }

    return result;
}

void ppmbtl_init(void) {
    rmt_ppm_config_t cfg = {
        .tx_gpio_num = CONFIG_PPM_BOOTLOADER_TX,
        .rx_gpio_num = CONFIG_PPM_BOOTLOADER_RX,
    };
    ESP_ERROR_CHECK(rmt_ppm_init(&cfg));
}

esp_err_t ppmbtl_enable(void) {
    return ESP_OK;
}

esp_err_t ppmbtl_disable(void) {
    return ESP_OK;
}

ppm_err_t ppmbtl_doAction(bool manpow,
                          bool broadcast,
                          uint32_t bitrate,
                          ppm_memory_t memory,
                          ppm_action_t action,
                          ihexContainer_t * ihex) {
    ppm_err_t retval = PPM_FAIL_UNKNOWN;

    if (ihex != NULL) {
        uint32_t pattern_time = 50000u;
        if (manpow) {
            pattern_time = 100000u;
        } else if (ppmbtl_chipPowered()) {
            ppmbtl_chipPower(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        const MlxChip_t * chip_info = NULL;
        retval = ppmbtl_enterProgrammingMode(broadcast, bitrate, pattern_time, &chip_info);

        if ((retval == PPM_OK) && (chip_info != NULL)) {
            if (memory == PPM_MEM_FLASH) {
                if (action == PPM_ACT_PROGRAM) {
                    retval = ppmbtl_programFlashMemory(chip_info, broadcast, ihex);
                } else if (action == PPM_ACT_VERIFY) {
                    retval = ppmbtl_verifyFlashMemory(chip_info, ihex);
                }
            } else if (memory == PPM_MEM_FLASH_CS) {
                if (chip_info->bootloaders.ppm_loader->flash_cs_programming_session) {
                    if (action == PPM_ACT_PROGRAM) {
                        retval = ppmbtl_programFlashCsMemory(chip_info, broadcast, ihex);
                    } else if (action == PPM_ACT_VERIFY) {
                        retval = ppmbtl_verifyFlashCsMemory(chip_info, ihex);
                    }
                } else {
                    retval = PPM_FAIL_ACTION_NOT_SUPPORTED;
                }
            } else if (memory == PPM_MEM_NVRAM) {
                if (action == PPM_ACT_PROGRAM) {
                    retval = ppmbtl_programEepromMemory(chip_info, broadcast, ihex);
                } else if (action == PPM_ACT_VERIFY) {
                    if (chip_info->bootloaders.ppm_loader->eeprom_verification_session) {
                        retval = ppmbtl_verifyEepromMemory(chip_info, ihex);
                    } else {
                        retval = PPM_FAIL_ACTION_NOT_SUPPORTED;
                    }
                }
            }
        }

        (void)ppmbtl_exitProgrammingMode(chip_info, broadcast);

        if (!manpow) {
            ppmbtl_chipPower(false);
        }
    } else {
        retval = PPM_FAIL_INV_HEX_FILE;
    }

    return retval;
}

void __attribute__((weak)) ppmbtl_chipPower(bool enable) {
    (void)enable;
}

bool __attribute__((weak)) ppmbtl_chipPowered(void) {
    return false;
}
