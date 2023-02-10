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
git -C ../esp-mbedtls/mbedtls diff > ../patches/nuttx/esp-mbedtls/mbedtls/0001-after_prefixer.patch


ctags --kinds-c=fv ../esp-mbedtls/mbedtls/library/*.c


./prefixer.sh ./tags ../esp-mbedtls/mbedtls/
 3306  ./prefixer.sh ./tags ../wpa_supplicant/esp_supplicant
 3307  history prefixer
 3308  history "prefixer"
 3309  ./prefixer.sh ./tags ../esp-mbedtls/port
 3312  ./prefixer.sh ./tags ../wpa_supplicant/port/