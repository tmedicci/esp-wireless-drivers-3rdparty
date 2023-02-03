############################################################################
# apps/wireless/esp-mbedtls/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

# Mbed TLS crypto library

# Set up build configuration and environment
ESP_CRT_BUNDLE_DIR = esp_crt_bundle
ESP_MBEDTLS_PORT_DIR = port
MBEDTLS_DIR = mbedtls
MBEDTLS_LIBRARY_DIR = $(MBEDTLS_DIR)$(DELIM)library

DEPPATH += --dep-path $(MBEDTLS_LIBRARY_DIR)
VPATH   += :$(MBEDTLS_LIBRARY_DIR)

# These should be removed os layer is present for apps
INCDIR += ../../../nuttx/arch/xtensa/src/esp32/esp-wireless-drivers-3rdparty/include ../../../nuttx/arch/xtensa/src/esp32/esp-wireless-drivers-3rdparty/include/esp32
CFLAGS += ${shell $(INCDIR) "$(CC)" $(INCDIR)}

# This lets Mbed TLS better use some of the POSIX features we have
CFLAGS += ${DEFINE_PREFIX}__unix__

CSRCS := $(notdir $(wildcard $(MBEDTLS_LIBRARY_DIR)$(DELIM)*.c))
CSRCS += $(wildcard $(ESP_CRT_BUNDLE_DIR)$(DELIM)*.c)
# CSRCS += $(wildcard $(ESP_MBEDTLS_PORT_DIR)$(DELIM)*.c)

CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)esp_hardware.c
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)esp_mem.c
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)esp_timing.c

# ifeq ($(CONFIG_SOC_AES_SUPPORTED),y)
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)aes/esp_aes_xts.c
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)aes/esp_aes_common.c
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)aes/block/esp_aes.c
# endif
CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)md/esp_md.c

# CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)sha/esp_sha.c

# specific for ESP32
# CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)sha/parallel_engine/sha.c
# CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)sha/parallel_engine/esp_sha1.c
# CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)sha/parallel_engine/esp_sha256.c
# CSRCS += $(ESP_MBEDTLS_PORT_DIR)$(DELIM)sha/parallel_engine/esp_sha512.c
# CSRCS += $(wildcard $(ESP_MBEDTLS_PORT_DIR)$(DELIM)esp32$(DELIM)*.c)

$(MBEDTLS_LIBRARY_DIR):
	$(Q) echo "mbedTLS dirname: $(MBEDTLS_LIBRARY_DIR)"
	$(Q) touch $(MBEDTLS_LIBRARY_DIR)

$(ESP_CRT_BUNDLE_DIR):
	$(Q) echo "esp_crt_bundle dirname: $(ESP_CRT_BUNDLE_DIR)"
	$(Q) touch $(ESP_CRT_BUNDLE_DIR)

context:: $(MBEDTLS_LIBRARY_DIR) $(ESP_CRT_BUNDLE_DIR)

# distclean::
# 	# $(call DELDIR, $(MBEDTLS_LIBRARY_DIR))
# 	# $(call DELDIR, $(ESP_CRT_BUNDLE_DIR))
# 	# $(call DELFILE, $(MBEDTLS_ZIP))

include $(APPDIR)/Application.mk
