#!/usr/bin/env bash

source $HOME/esp/esp-idf/export.sh

result="$?"

if [ "${result}" -ne '0' ]; then
    echo "ESP-IDF path not found. Please check '$HOME/esp/esp-idf/'"
    exit 1
fi

root_dir="$(git rev-parse --show-toplevel)"

idf_version="$(git -C ${IDF_PATH} describe)"
echo "Copying sources from local ESP-IDF ${idf_version}"

echo "Copying wpa_supplicant from ESP-IDF"
rm -rf "${root_dir}/wpa_supplicant"
cp -r "${IDF_PATH}/components/wpa_supplicant" "${root_dir}"
