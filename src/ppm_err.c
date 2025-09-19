/**
 * @file
 * @brief The Melexis error module.
 * @internal
 *
 * @copyright (C) 2024-2025 Melexis N.V.
 *
 * Melexis N.V. is supplying this code for use with Melexis N.V. processor based microcontrollers only.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.  MELEXIS N.V. SHALL NOT IN ANY CIRCUMSTANCES,
 * BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * @endinternal
 *
 * @ingroup application
 *
 * @details This file contains the implementations of the Melexis error module.
 */
#include <string.h>

#include "ppm_err.h"

const struct {
    ppm_err_t code;
    const char *name;
} ppmErrorCodesDict[] = {
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

const char *ppmerr_ErrorCodeToName(ppm_err_t code) {
    size_t i;

    for (i = 0; i < sizeof(ppmErrorCodesDict) / sizeof(ppmErrorCodesDict[0]); ++i) {
        if (ppmErrorCodesDict[i].code == code) {
            return ppmErrorCodesDict[i].name;
        }
    }

    return NULL;
}
