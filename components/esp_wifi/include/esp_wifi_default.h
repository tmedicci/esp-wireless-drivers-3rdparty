// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _ESP_WIFI_DEFAULT_H
#define _ESP_WIFI_DEFAULT_H

/**
 * @brief Sets default wifi event handlers for STA interface
 *
 * @param esp_netif instance of corresponding if object
 *
 * @return
 *  - ESP_OK on success, error returned from esp_event_handler_register if failed
 */
esp_err_t esp_wifi_set_default_wifi_sta_handlers(void *esp_netif);

/**
 * @brief Sets default wifi event handlers for STA interface
 *
 * @param esp_netif instance of corresponding if object
 *
 * @return
 *  - ESP_OK on success, error returned from esp_event_handler_register if failed
 */
esp_err_t esp_wifi_set_default_wifi_driver_and_handlers(wifi_interface_t wifi_if, void *esp_netif);

esp_err_t esp_wifi_set_default_wifi_ap_handlers(void *esp_netif);

/**
 * @brief Clears default wifi event handlers for supplied network interface
 *
 * @param esp_netif instance of corresponding if object
 *
 * @return
 *  - ESP_OK on success, error returned from esp_event_handler_register if failed
 */
esp_err_t esp_wifi_clear_default_wifi_driver_and_handlers(void *esp_netif);


#endif //_ESP_WIFI_DEFAULT_H