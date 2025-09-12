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
 *
 * @details Definitions of the PPM bootloader module.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gen_bootloader.h"
#include "intelhex.h"
#include "mlx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** initialize the PPM bootloader module */
void ppmbtl_init(void);

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
mlx_err_t ppmbtl_doAction(bool manpow,
                          bool broadcast,
                          uint32_t bitrate,
                          btl_memory_t memory,
                          btl_action_t action,
                          ihexContainer_t * ihex);

#ifdef __cplusplus
}
#endif
