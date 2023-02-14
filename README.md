# Espressif Wireless Framework

## Introduction

This project is used to integrate ESP32 family SoC's wireless software drivers into other platforms, like NuttX.

Wireless software drivers mainly contains of hardware drivers, wireless protocols and utils.

## How to Use this Repository

### mbedTLS Symbol Collisions 

cd esp-idf/components/mbedtls/mbedtls
ctags --kinds-c=fv esp-idf/components/mbedtls/mbedtls/library/*.c

#### Generate Patch for mbedtls

cd ../utils/
git -C ../esp-idf reset --hard
git -C ../esp-idf/components/mbedtls/mbedtls reset --hard

./prefixer.sh ctags/mbedtls/tags ../esp-idf/components/mbedtls/mbedtls
mkdir -p ../nuttx/patches/esp-idf/components/mbedtls/mbedtls
git -C ../esp-idf/components/mbedtls/mbedtls diff --full-index --binary > ../nuttx/patches/esp-idf/components/mbedtls/mbedtls/0001-add_prefix.patch
git -C ../esp-idf/components/mbedtls/mbedtls reset --hard

#### Generate Patch for ESP-IDF's mbedtls port

git -C ../esp-idf reset --hard
./prefixer.sh ctags/mbedtls/tags ../esp-idf/components/mbedtls/port
mkdir -p ../nuttx/patches/esp-idf/components/mbedtls/port
git -C ../esp-idf diff --full-index --binary > ../nuttx/patches/esp-idf/components/mbedtls/port/0001-add_prefix.patch

#### Generate Patch for wpa_supplicant

git -C ../esp-idf reset --hard
./prefixer.sh ctags/mbedtls/tags ../esp-idf/components/wpa_supplicant
mkdir -p ../nuttx/patches/esp-idf/components/wpa_supplicant
git -C ../esp-idf/ diff --full-index --binary > ../nuttx/patches/esp-idf/components/wpa_supplicant/0001-add_prefix.patch

ctags --kinds-c=fv ../esp-mbedtls/mbedtls/library/*.c
./prefixer.sh ./tags ../esp-mbedtls/mbedtls/
 3306  ./prefixer.sh ./tags ../wpa_supplicant/esp_supplicant
 3307  history prefixer
 3308  history "prefixer"
 3309  ./prefixer.sh ./tags ../esp-mbedtls/port
 3312  ./prefixer.sh ./tags ../wpa_supplicant/port/