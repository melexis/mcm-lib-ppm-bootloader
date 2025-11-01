/**
 * @file
 * @brief PPM bootloader definitions.
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
 * @addtogroup lib_ppm_bootloader PPM Bootloader Library
 *
 * @details Definitions of the PPM bootloader module.
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "intelhex.h"

#include "ppm_err.h"
#include "ppm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** initialize the PPM bootloader module */
void ppmbtl_init(void);

/** enable the ppm interface
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t ppmbtl_enable(void);

/** disable the ppm interface
 *
 * @returns  error code representing the result of the action.
 */
esp_err_t ppmbtl_disable(void);

/** perform a full programming/verification action to the connected chip
 *
 * @param[in]  manpow  enable manual power cycling.
 * @param[in]  broadcast  enable broadcast mode during upload.
 * @param[in]  bitrate  bitrate to be used during bootloader operations.
 * @param[in]  memory  memory type to perform action on.
 * @param[in]  action  action type to perform.
 * @param[in]  ihex  intel hex container to perform action with.
 * @returns  error code representing the result of the action.
 */
ppm_err_t ppmbtl_doAction(bool manpow,
                          bool broadcast,
                          uint32_t bitrate,
                          ppm_memory_t memory,
                          ppm_action_t action,
                          ihexContainer_t * ihex);

/** library callout to en/disable the chip power
 *
 * @param[in]  enable  whether to enable the chip power.
 */
void ppmbtl_chipPower(bool enable);

/** library callout to check whether the chip is powered
 *
 * @retval  true  chip is currently powered.
 * @retval  false  otherwise
 */
bool ppmbtl_chipPowered(void);

/** @} */

#ifdef __cplusplus
}
#endif
