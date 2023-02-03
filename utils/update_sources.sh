#!/usr/bin/env bash

source $HOME/esp/esp-idf/export.sh

result="$?"

if [ "${result}" -ne '0' ]; then
    echo "ESP-IDF path not found. Please check '$HOME/esp/esp-idf/'"
    exit 1
fi

root_dir="$(git rev-parse --show-toplevel)"

idf_version="$(git -C ${IDF_PATH} describe)"
echo "Copying sources from local ESP-IDF ${idf_version}."

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
git submodule update --recursive --init
git -C "${root_dir}/esp-mbedtls/mbedtls" checkout "${mbedtls_hash}"
