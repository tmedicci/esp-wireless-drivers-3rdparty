#!/usr/bin/env bash

source $HOME/esp/esp-idf/export.sh

result="$?"

if [ "${result}" -ne '0' ]; then
    echo "ESP-IDF path not found. Please check '$HOME/esp/esp-idf/'"
    exit 1
fi

root_dir="$(git rev-parse --show-toplevel)"
idf_tag="$(git -C ${IDF_PATH} describe)"
idf_commit_hash="$(git -C ${IDF_PATH} rev-parse HEAD)"

echo "Copying sources from local ESP-IDF ${idf_tag}."
echo "${idf_tag} commit ${idf_commit_hash}" > "${root_dir}/version"

echo "Copying Wi-Fi library from ESP-IDF..."
wifi_lib_hash="$(git -C ${IDF_PATH}/components/esp_wifi/lib rev-parse HEAD)"
echo "ESP32 Wi-Fi lib submodule: ${wifi_lib_hash}"
rm -rf "${root_dir}/libs/"

echo "Copying wpa_supplicant from ESP-IDF..."
rm -rf "${root_dir}/wpa_supplicant"
cp -r "${IDF_PATH}/components/wpa_supplicant" "${root_dir}"

echo "Copying mbedtls from ESP-IDF..."
mbedtls_hash="$(git -C ${IDF_PATH}/components/mbedtls/mbedtls rev-parse HEAD)"
echo "mbedTLS submodule: ${mbedtls_hash}"
rm -rf "${root_dir}/esp-mbedtls"
cp -r "${IDF_PATH}/components/mbedtls" "${root_dir}"
mv "${root_dir}/mbedtls" "${root_dir}/esp-mbedtls"
rm -rf "${root_dir}/esp-mbedtls/mbedtls"

echo "Copying include files from ESP-IDF..."
cp -r "${IDF_PATH}/components/esp_wifi/include" "${root_dir}"
cp -r "${IDF_PATH}/components/esp_phy/esp32s3/include/phy_init_data.h" "${root_dir}/include/esp32s3/phy_init_data.h"
cp -r "${IDF_PATH}/components/esp_rom/include/esp32s3/rom/ets_sys.h" "${root_dir}/include/esp32s3/rom/ets_sys.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/ledc_caps.h" "${root_dir}/include/esp32s3/soc/ledc_caps.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/mpu_caps.h" "${root_dir}/include/esp32s3/soc/mpu_caps.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/reg_base.h" "${root_dir}/include/esp32s3/soc/reg_base.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/soc.h" "${root_dir}/include/esp32s3/soc/soc.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/soc_caps.h" "${root_dir}/include/esp32s3/soc/soc_caps.h"
cp -r "${IDF_PATH}/components/soc/esp32s3/include/soc/twai_caps.h" "${root_dir}/include/esp32s3/soc/twai_caps.h"
cp -r "${IDF_PATH}/components/esp_common/include" "${root_dir}"
cp -r "${IDF_PATH}/components/esp_hw_support/include/esp_cpu.h" "${root_dir}/include/esp_cpu.h"
cp -r "${IDF_PATH}/components/esp_hw_support/include/esp_interface.h" "${root_dir}/include/esp_interface.h"
cp -r "${IDF_PATH}/components/esp_hw_support/include/esp_mac.h" "${root_dir}/include/esp_mac.h"
cp -r "${IDF_PATH}/components/esp_hw_support/include/esp_random.h" "${root_dir}/include/esp_random.h"
cp -r "${IDF_PATH}/components/esp_event/include" "${root_dir}"
cp -r "${IDF_PATH}/components/esp_phy/include" "${root_dir}"
cp -r "${IDF_PATH}/components/esp_hw_support/include/esp_private" "${root_dir}/include"
cp -r "${IDF_PATH}/components/esp_rom/include/esp_rom_md5.h" "${root_dir}/include/esp_rom_md5.h"
cp -r "${IDF_PATH}/components/esp_system/include/esp_system.h" "${root_dir}/include/esp_system.h"
cp -r "${IDF_PATH}/components/esp_timer/include/esp_timer.h" "${root_dir}/include/esp_timer.h"
cp -r "${IDF_PATH}/components/nvs_flash/include/nvs.h" "${root_dir}/include/nvs.h"

git submodule update --init --recursive
git -C "${root_dir}/libs" checkout "${wifi_lib_hash}"
git -C "${root_dir}/esp-mbedtls/mbedtls" checkout "${mbedtls_hash}"
