/**
 * \brief AES block cipher, ESP block hardware accelerated version
 * Based on mbedTLS FIPS-197 compliant version.
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Additions Copyright (C) 2016-2017, Espressif Systems (Shanghai) PTE Ltd
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
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */
#include <string.h>

#include <nuttx/crypto/crypto.h>

#include "mbedtls/aes.h"
#include "mbedtls/platform_util.h"
#include "aes/esp_aes.h"
#include "soc/hwcrypto_periph.h"
#include "hal/aes_hal.h"
#include "aes/esp_aes_internal.h"

#include <stdio.h>

#include "esp32_aes.h"

/* AES uses a spinlock mux not a lock as the underlying block operation
   only takes 208 cycles (to write key & compute block), +600 cycles
   for DPORT protection but +3400 cycles again if you use a full sized lock.

   For CBC, CFB, etc. this may mean that interrupts are disabled for a longer
   period of time for bigger lengths. However at the moment this has to happen
   anyway due to DPORT protection...
*/
// static spinlock_t aes_spinlock;
// static irqstate_t aes_spinlock_irqstate;

void esp_aes_acquire_hardware( void )
{
    // aes_spinlock_irqstate = spin_lock_irqsave(&aes_spinlock);
    // /* Enable AES hardware */
    // periph_module_enable(PERIPH_AES_MODULE);
}

void esp_aes_release_hardware( void )
{
    // /* Disable AES hardware */
    // periph_module_disable(PERIPH_AES_MODULE);

    // spin_unlock_irqrestore(&aes_spinlock, aes_spinlock_irqstate);
}



/* Run a single 16 byte block of AES, using the hardware engine.
 *
 * Call only while holding esp_aes_acquire_hardware().
 */
#if 0
static int esp_aes_block(esp_aes_context *ctx, const void *input, void *output)
{
    uint32_t i0, i1, i2, i3;
    const uint32_t *input_words = (uint32_t *)input;
    uint32_t *output_words = (uint32_t *)output;

    /* If no key is written to hardware yet, either the user hasn't called
       mbedtls_aes_setkey_enc/mbedtls_aes_setkey_dec - meaning we also don't
       know which mode to use - or a fault skipped the
       key write to hardware. Treat this as a fatal error and zero the output block.
    */
    if (ctx->key_in_hardware != ctx->key_bytes) {
        bzero(output, 16);
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;
    }
    i0 = input_words[0];
    i1 = input_words[1];
    i2 = input_words[2];
    i3 = input_words[3];

    aes_hal_transform_block(input, output);

    /* Physical security check: Verify the AES accelerator actually ran, and wasn't
       skipped due to external fault injection while starting the peripheral.

       Note that i0,i1,i2,i3 are copied from input buffer in case input==output.

       Bypassing this check requires at least one additional fault.
    */
    if (i0 == output_words[0] && i1 == output_words[1] && i2 == output_words[2] && i3 == output_words[3]) {
        // calling zeroing functions to narrow the
        // window for a double-fault of the abort step, here
        memset(output, 0, 16);
        mbedtls_platform_zeroize(output, 16);
        abort();
    }

    return 0;
}
#endif

void esp_aes_encrypt(esp_aes_context *ctx,
                     const unsigned char input[16],
                     unsigned char output[16] )
{
    esp_internal_aes_encrypt(ctx, input, output);
}

/*
 * AES-ECB block encryption
 */
int esp_internal_aes_encrypt(esp_aes_context *ctx,
                             const unsigned char input[16],
                             unsigned char output[16] )
{
    int r;

    if (!valid_key_length(ctx)) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    esp_aes_acquire_hardware();
    ctx->key_in_hardware = 0;
    r = aes_cypher(output, input, 16, NULL, ctx->key, ctx->key_bytes,
                    AES_MODE_ECB, CYPHER_ENCRYPT);
    if (!r) {
        ctx->key_in_hardware = ctx->key_bytes;  
    }
    esp_aes_release_hardware();
    return r;
}

void esp_aes_decrypt(esp_aes_context *ctx,
                     const unsigned char input[16],
                     unsigned char output[16] )
{
    esp_internal_aes_decrypt(ctx, input, output);
}

/*
 * AES-ECB block decryption
 */

int esp_internal_aes_decrypt(esp_aes_context *ctx,
                             const unsigned char input[16],
                             unsigned char output[16] )
{
    int r;

    if (!valid_key_length(ctx)) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    esp_aes_acquire_hardware();
    ctx->key_in_hardware = 0;
    r = aes_cypher(output, input, 16, NULL, ctx->key, ctx->key_bytes,
                    AES_MODE_ECB, CYPHER_DECRYPT);
    if (!r) {
        ctx->key_in_hardware = 16;  
    }
    esp_aes_release_hardware();
    return r;
}

/*
 * AES-ECB block encryption/decryption
 */
int esp_aes_crypt_ecb(esp_aes_context *ctx,
                      int mode,
                      const unsigned char input[16],
                      unsigned char output[16] )
{
    int r;

    if (!valid_key_length(ctx)) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    esp_aes_acquire_hardware();
    ctx->key_in_hardware = 0;
    r = aes_cypher(output, input, 16, NULL, ctx->key, ctx->key_bytes,
                    AES_MODE_ECB, CYPHER_DECRYPT);
    if (!r) {
        ctx->key_in_hardware = ctx->key_bytes;
    }
    esp_aes_release_hardware();

    return r;
}


/*
 * AES-CBC buffer encryption/decryption
 */
int esp_aes_crypt_cbc(esp_aes_context *ctx,
                      int mode,
                      size_t length,
                      unsigned char iv[16],
                      const unsigned char *input,
                      unsigned char *output )
{
    uint32_t *output_words = (uint32_t *)output;
    const uint32_t *input_words = (const uint32_t *)input;
    uint32_t *iv_words = (uint32_t *)iv;
    int ret;

    if ( length % 16 ) {
        return ( ERR_ESP_AES_INVALID_INPUT_LENGTH );
    }

    if (!valid_key_length(ctx)) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }

    esp_aes_acquire_hardware();
    ctx->key_in_hardware = 0;
    ret = aes_cypher(output, input, length, iv, ctx->key, ctx->key_bytes,
                    AES_MODE_CBC, mode);
    if (!ret) {
        ctx->key_in_hardware = length;  
    }
    esp_aes_release_hardware();

    return ret;
}
