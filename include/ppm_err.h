/**
 * @file
 * @brief PPM bootloader error code definitions.
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
 * @details Definitions of the PPM bootloader error codes.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** PPM bootloader error code enum */
typedef enum ppm_error_e {
    PPM_OK = 0,                                 /**< operation was successful */
    PPM_FAIL_UNKNOWN = -1,                      /**< */
    PPM_FAIL_INTERNAL = -2,                     /**< */
    PPM_FAIL_SET_BAUD = -16,                    /**< failed setting new baudrate */
    PPM_FAIL_BTL_ENTER_PPM_MODE = -17,          /**< failed entering ppm mode */
    PPM_FAIL_CALIBRATION = -18,                 /**< */
    PPM_FAIL_UNLOCK = -19,                      /**< */
    PPM_FAIL_CHIP_NOT_SUPPORTED = -20,          /**< */
    PPM_FAIL_ACTION_NOT_SUPPORTED = -21,        /**< */
    PPM_FAIL_INV_HEX_FILE = -22,                /**< */
    PPM_FAIL_MISSING_DATA = -23,                /**< */
    PPM_FAIL_PROGRAMMING_FAILED = -24,          /**< */
    PPM_FAIL_VERIFY_FAILED = -25,               /**< */
} ppm_err_t;                                    /**< PPM bootloader error code type */

/** convert a ppm bootloader error code in a human readable message
 *
 * @returns  human readable error message.
 */
const char *ppmerr_ErrorCodeToName(ppm_err_t code);

#ifdef __cplusplus
}
#endif
