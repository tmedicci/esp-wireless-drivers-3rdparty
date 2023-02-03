/*
 *  ESP32 hardware accelerated SHA1/256/512 implementation
 *  based on mbedTLS FIPS-197 compliant version.
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Additions Copyright (C) 2016, Espressif Systems (Shanghai) PTE Ltd
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
/*
 *  The SHA-1 standard was published by NIST in 1993.
 *
 *  http://www.itl.nist.gov/fipspubs/fip180-1.htm
 */

#include <string.h>
#include <stdio.h>
#include <machine/endian.h>
#include <assert.h>

#include <nuttx/spinlock.h>

#include "hal/sha_hal.h"
#include "hal/sha_types.h"
#include "sha/sha_parallel_engine.h"
#include "soc/hwcrypto_periph.h"
#include "esp_private/periph_ctrl.h"

/****************************************************************************
* Private Types
****************************************************************************/
typedef sem_t portMUX_TYPE;
typedef sem_t *SemaphoreHandle_t;
typedef sem_t _lock_t;
typedef uint32_t        TickType_t;
typedef int32_t         BaseType_t;

/******************************************************************************
* macro defination
******************************************************************************/
#define ESP_OK OK
#define ESP_ERR_INVALID_ARG ERROR
#define ESP_ERR_INVALID_STATE ERROR
#define ESP_FAIL ERROR
#define ESP_ERR_NO_MEM ERROR
#define ESP_ERR_TIMEOUT ERROR

#define pdFALSE 0
#define pdTRUE 1

#ifdef CONFIG_PRIORITY_INHERITANCE
#define portMUX_INITIALIZER_LOCKED { .semcount = 0, .flags = FLAGS_INITIALIZED, }
#define portMUX_INITIALIZER_UNLOCKED { .semcount = 1, .flags = FLAGS_INITIALIZED, }
#else
#define portMUX_INITIALIZER_LOCKED { .semcount = 0 }
#define portMUX_INITIALIZER_UNLOCKED { .semcount = 1 }
#endif

#define portENTER_CRITICAL(lock) sem_wait(lock)
#define portEXIT_CRITICAL(lock) sem_post(lock)

#define xSemaphoreTake(sem, t) sem_wait(sem)
#define xSemaphoreGive(sem) sem_post(sem)

#define vTaskDelay(t)   usleep(t)

#define portMAX_DELAY       0xffffffff

/****************************************************************************
* Private Functions
****************************************************************************/

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
	SemaphoreHandle_t x = kmm_malloc(sizeof(sem_t));
	if (x != NULL) {
		sem_init(x, 0, 1);
		sem_setprotocol(x, SEM_PRIO_NONE);
	}
	return x;
}

void vSemaphoreDelete(SemaphoreHandle_t sem)
{
	sem_destroy(sem);
}


/*
     Single spinlock for SHA engine memory block
*/
static spinlock_t memory_block_lock;
static irqstate_t memory_block_lockirqstate;

/* Binary semaphore managing the state of each concurrent SHA engine.

   Available = noone is using this SHA engine
   Taken = a SHA session is running on this SHA engine

   Indexes:
   0 = SHA1
   1 = SHA2_256
   2 = SHA2_384 or SHA2_512
*/
static SemaphoreHandle_t engine_states[3];

static uint8_t engines_in_use;

/* Spinlock for engines_in_use counter
*/
static portMUX_TYPE engines_in_use_lock = portMUX_INITIALIZER_UNLOCKED;

/* Return block size (in words) for a given SHA type */
inline static size_t block_length(esp_sha_type type)
{
    switch (type) {
    case SHA1:
    case SHA2_256:
        return 64 / 4;
    case SHA2_384:
    case SHA2_512:
        return 128 / 4;
    default:
        return 0;
    }
}

/* Index into the engine_states array */
inline static size_t sha_engine_index(esp_sha_type type)
{
    switch (type) {
    case SHA1:
        return 0;
    case SHA2_256:
        return 1;
    default:
        return 2;
    }
}

void esp_sha_lock_memory_block(void)
{
    memory_block_lockirqstate = spin_lock_irqsave(&memory_block_lock);
}

void esp_sha_unlock_memory_block(void)
{
    spin_unlock_irqrestore(&memory_block_lock, memory_block_lockirqstate);
}

static SemaphoreHandle_t sha_get_engine_state(esp_sha_type sha_type)
{
    unsigned idx = sha_engine_index(sha_type);
    volatile SemaphoreHandle_t *engine = &engine_states[idx];
    SemaphoreHandle_t result = *engine;

    if (result == NULL) {
        // Create a new semaphore for 'in use' flag
        SemaphoreHandle_t new_engine = xSemaphoreCreateBinary();
        assert(new_engine != NULL);
        xSemaphoreGive(new_engine); // start available

        (*engine) = new_engine;

        result = *engine;
    }
    return result;
}

static bool esp_sha_lock_engine_common(esp_sha_type sha_type, TickType_t ticks_to_wait);

bool esp_sha_try_lock_engine(esp_sha_type sha_type)
{
    return esp_sha_lock_engine_common(sha_type, 0);
}

void esp_sha_lock_engine(esp_sha_type sha_type)
{
    esp_sha_lock_engine_common(sha_type, portMAX_DELAY);
}

static bool esp_sha_lock_engine_common(esp_sha_type sha_type, TickType_t ticks_to_wait)
{
    SemaphoreHandle_t engine_state = sha_get_engine_state(sha_type);
    BaseType_t result = xSemaphoreTake(engine_state, ticks_to_wait);

    if (result == pdFALSE) {
        // failed to take semaphore
        return false;
    }

    portENTER_CRITICAL(&engines_in_use_lock);

    if (engines_in_use == 0) {
        /* Just locked first engine,
           so enable SHA hardware */
        periph_module_enable(PERIPH_SHA_MODULE);
    }

    engines_in_use++;
    assert(engines_in_use <= 3);

    portEXIT_CRITICAL(&engines_in_use_lock);

    return true;
}


void esp_sha_unlock_engine(esp_sha_type sha_type)
{
    SemaphoreHandle_t engine_state = sha_get_engine_state(sha_type);

    portENTER_CRITICAL(&engines_in_use_lock);

    engines_in_use--;

    if (engines_in_use == 0) {
        /* About to release last engine, so
           disable SHA hardware */
        periph_module_disable(PERIPH_SHA_MODULE);
    }

    portEXIT_CRITICAL(&engines_in_use_lock);

    xSemaphoreGive(engine_state);
}

void esp_sha_read_digest_state(esp_sha_type sha_type, void *digest_state)
{
#ifndef NDEBUG
    {
        SemaphoreHandle_t engine_state = sha_get_engine_state(sha_type);
        assert(uxSemaphoreGetCount(engine_state) == 0 &&
               "SHA engine should be locked" );
    }
#endif

    // preemptively do this before entering the critical section, then re-check once in it
    sha_hal_wait_idle();

    esp_sha_lock_memory_block();

    sha_hal_read_digest(sha_type, digest_state);

    esp_sha_unlock_memory_block();
}

void esp_sha_block(esp_sha_type sha_type, const void *data_block, bool first_block)
{
#ifndef NDEBUG
    {
        SemaphoreHandle_t engine_state = sha_get_engine_state(sha_type);
        assert(uxSemaphoreGetCount(engine_state) == 0 &&
               "SHA engine should be locked" );
    }
#endif

    // preemptively do this before entering the critical section, then re-check once in it
    sha_hal_wait_idle();
    esp_sha_lock_memory_block();

    sha_hal_hash_block(sha_type, data_block, block_length(sha_type), first_block);

    esp_sha_unlock_memory_block();
}
