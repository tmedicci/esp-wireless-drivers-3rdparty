# Espressif Wireless Framework

## Introduction

This project is used to integrate ESP32 family SoC's wireless software drivers into other platforms, like NuttX.

Wireless software drivers mainly contains of hardware drivers, wireless protocols and utils.

## How to Use this Repository

### mbedTLS Symbol Collisions 

cd esp-mbedtls
ctags --kinds-c=fv mbedtls/library/*.c
cd ../utils/
./prefixer.sh ../esp-mbedtls/tags ../esp-mbedtls/mbedtls/
git -C ../esp-mbedtls/mbedtls diff --full-index --binary > ../patches/nuttx/esp-mbedtls/mbedtls/0001-after_prefixer.patch
./prefixer.sh ../esp-mbedtls/tags ../wpa_supplicant
git diff --full-index --binary > ../patches/nuttx/0001-wpa_supplicant_after_prefixer.patch
git add ../wpa_supplicant/*
./prefixer.sh ../esp-mbedtls/tags ../esp-mbedtls/port
git diff > ../patches/nuttx/0002-esp-mbedtls_port_after_prefixer.patch


ctags --kinds-c=fv ../esp-mbedtls/mbedtls/library/*.c
./prefixer.sh ./tags ../esp-mbedtls/mbedtls/
 3306  ./prefixer.sh ./tags ../wpa_supplicant/esp_supplicant
 3307  history prefixer
 3308  history "prefixer"
 3309  ./prefixer.sh ./tags ../esp-mbedtls/port
 3312  ./prefixer.sh ./tags ../wpa_supplicant/port/