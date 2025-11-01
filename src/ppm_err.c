/**
 * @file
 * @brief PPM bootloader error code module.
 * @internal
 *
 * @copyright (C) 2024-2025 Melexis N.V.
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
 * @details Implementations of the PPM bootloader error code module.
 */
#include <stddef.h>

#include "ppm_err.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const struct {
    ppm_err_t code;
    const char *name;
} ppm_error_codes_dict[] = {
    {PPM_OK, "operation was successful"},
    {PPM_FAIL_UNKNOWN, "unknown error"},
    {PPM_FAIL_INTERNAL, "internal error"},
    {PPM_FAIL_SET_BAUD, "failed setting new baudrate"},
    {PPM_FAIL_BTL_ENTER_PPM_MODE, "failed entering ppm mode"},
    {PPM_FAIL_CALIBRATION, "failed sending calibration frame"},
    {PPM_FAIL_UNLOCK, "failed unlocking session mode"},
    {PPM_FAIL_CHIP_NOT_SUPPORTED, "connected chip is not supported"},
    {PPM_FAIL_ACTION_NOT_SUPPORTED, "action is not supported"},
    {PPM_FAIL_INV_HEX_FILE, "hex file could not be read"},
    {PPM_FAIL_MISSING_DATA, "no data for the memory in the hex file"},
    {PPM_FAIL_PROGRAMMING_FAILED, "programming failed"},
    {PPM_FAIL_VERIFY_FAILED, "verification failed"},
};

const char *ppm_err_to_string(ppm_err_t code) {
    for (size_t i = 0; i < ARRAY_SIZE(ppm_error_codes_dict); ++i) {
        if (ppm_error_codes_dict[i].code == code) {
            return ppm_error_codes_dict[i].name;
        }
    }

    return "Unknown error";
}
