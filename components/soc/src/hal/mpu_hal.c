// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <stdint.h>

#include "esp_err.h"

#include "hal/mpu_hal.h"
#include "hal/mpu_ll.h"
#include "hal/mpu_types.h"

#include "soc/mpu_caps.h"

#define CHECK(cond)         {  if (!(cond)) abort(); }

esp_err_t mpu_hal_set_region_access(int id, mpu_access_t access)
{
    CHECK(id < SOC_MPU_REGIONS_MAX_NUM && id >= 0);
    CHECK(
#if SOC_MPU_REGION_RO_SUPPORTED
           access == MPU_REGION_RO ||
#endif
#if SOC_MPU_REGION_WO_SUPPORTED
           access == MPU_REGION_WO ||
#endif
           access == MPU_REGION_RW ||
           access == MPU_REGION_X  ||
           access == MPU_REGION_RWX ||
           access == MPU_REGION_ILLEGAL);

    uint32_t addr = cpu_ll_id_to_addr(id);

    switch (access)
    {
#if SOC_MPU_REGION_RO_SUPPORTED
        case MPU_REGION_RO:
            mpu_ll_set_region_ro(addr);
            break;
#endif
#if SOC_MPU_REGION_WO_SUPPORTED
        case MPU_REGION_WO:
            mpu_ll_set_region_wo(addr);
            break;
#endif
        case MPU_REGION_RW:
            mpu_ll_set_region_rw(addr);
            break;
        case MPU_REGION_X:
            mpu_ll_set_region_x(addr);
            break;
        case MPU_REGION_RWX:
            mpu_ll_set_region_rwx(addr);
            break;
        default:
            break;
    }

    return ESP_OK;
}