// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "esp_attr.h"
#include "esp_err.h"

#include "esp_log.h"
#include "esp_system.h"

#include "esp_clk_internal.h"

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
#include "esp32/cache_err_int.h"
#include "esp32/rom/cache.h"
#include "esp32/rom/rtc.h"
#include "esp32/rom/uart.h"
#include "esp32/spiram.h"
#include "esp32/rom/ets_sys.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/brownout.h"
#include "esp32s2/cache_err_int.h"
#include "esp32s2/rom/cache.h"
#include "esp32s2/rom/ets_sys.h"
#include "esp32s2/rom/rtc.h"
#include "esp32s2/spiram.h"
#include "esp32s2/rom/uart.h"
#include "soc/periph_defs.h"
#include "esp32s2/dport_access.h"
#include "esp32s2/memprot.h"
#endif

#include "bootloader_flash_config.h"
#include "esp_private/crosscore_int.h"
#include "esp_flash_encrypt.h"

#include "hal/rtc_io_hal.h"
#include "soc/dport_reg.h"
#include "soc/efuse_reg.h"
#include "soc/cpu.h"

#include "trax.h"

#include "bootloader_mem.h"

#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_APP_BUILD_TYPE_ELF_RAM
#include "esp32/rom/efuse.h"
#include "esp32/rom/spi_flash.h"
#endif // CONFIG_APP_BUILD_TYPE_ELF_RAM
#endif

#include "startup_internal.h"

extern int _bss_start;
extern int _bss_end;
extern int _rtc_bss_start;
extern int _rtc_bss_end;

extern int _init_start;

static const char *TAG = "cpu_start";

#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
extern int _ext_ram_bss_start;
extern int _ext_ram_bss_end;
#endif
#ifdef CONFIG_ESP32_IRAM_AS_8BIT_ACCESSIBLE_MEMORY
extern int _iram_bss_start;
extern int _iram_bss_end;
#endif
#endif // CONFIG_IDF_TARGET_ESP32

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
static volatile bool s_cpu_up[SOC_CPU_CORES_NUM] = { false };
static volatile bool s_cpu_inited[SOC_CPU_CORES_NUM] = { false };

static volatile bool s_resume_cores;
#endif

// If CONFIG_SPIRAM_IGNORE_NOTFOUND is set and external RAM is not found or errors out on testing, this is set to false.
bool g_spiram_ok = true;

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
void startup_resume_other_cores(void)
{
    s_resume_cores = true;
}


void IRAM_ATTR call_start_cpu1(void)
{
    cpu_hal_set_vecbase(&_init_start);

    ets_set_appcpu_boot_addr(0);

    bootloader_init_mem();

#if CONFIG_ESP_CONSOLE_UART_NONE
    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
#else // CONFIG_ESP_CONSOLE_UART_NONE
    uartAttach();
    ets_install_uart_printf();
    uart_tx_switch(CONFIG_ESP_CONSOLE_UART_NUM);
#endif

    DPORT_REG_SET_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_PDEBUG_ENABLE | DPORT_APP_CPU_RECORD_ENABLE);
    DPORT_REG_CLR_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_RECORD_ENABLE);

    s_cpu_up[1] = true;
    ESP_EARLY_LOGI(TAG, "App cpu up.");

    //Take care putting stuff here: if asked, FreeRTOS will happily tell you the scheduler
    //has started, but it isn't active *on this CPU* yet.
    esp_cache_err_int_init();

#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_ESP32_TRAX_TWOBANKS
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif
#endif

    s_cpu_inited[1] = true;

    while(!s_resume_cores) {
        cpu_hal_delay_us(100);
    }

    SYS_STARTUP_FN();
}

static void start_other_core(void)
{
    // If not the single core variant of ESP32 - check this since there is
    // no separate soc_caps.h for the single core variant.
    if (!REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_VER_DIS_APP_CPU)) {
        ESP_EARLY_LOGI(TAG, "Starting app cpu, entry point is %p", call_start_cpu1);

        Cache_Flush(1);
        Cache_Read_Enable(1);
        esp_cpu_unstall(1);

        // Enable clock and reset APP CPU. Note that OpenOCD may have already
        // enabled clock and taken APP CPU out of reset. In this case don't reset
        // APP CPU again, as that will clear the breakpoints which may have already
        // been set.
        if (!DPORT_GET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN)) {
            DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
            DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_C_REG, DPORT_APPCPU_RUNSTALL);
            DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
            DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
        }
        ets_set_appcpu_boot_addr((uint32_t)call_start_cpu1);

        volatile bool cpus_up = false;

        while(!cpus_up){
            cpus_up = true;
            for (int i = 0; i < SOC_CPU_CORES_NUM; i++) {
                cpus_up &= s_cpu_up[i];
            }
            cpu_hal_delay_us(100);
        }
    }
    else {
        s_cpu_inited[1] = true;
		ESP_EARLY_LOGI(TAG, "Single core mode");
		DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
    }
}
#endif // !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE

static void intr_matrix_clear(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Clear all the interrupt matrix register
    for (int i = ETS_WIFI_MAC_INTR_SOURCE; i <= ETS_CACHE_IA_INTR_SOURCE; i++) {
#elif CONFIG_IDF_TARGET_ESP32S2
    for (int i = ETS_WIFI_MAC_INTR_SOURCE; i < ETS_MAX_INTR_SOURCE; i++) {
#endif
        intr_matrix_set(0, i, ETS_INVALID_INUM);
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        intr_matrix_set(1, i, ETS_INVALID_INUM);
#endif
    }
}

/*
 * We arrive here after the bootloader finished loading the program from flash. The hardware is mostly uninitialized,
 * and the app CPU is in reset. We do have a stack, so we can do the initialization in C.
 */
void IRAM_ATTR call_start_cpu0(void)
{
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    RESET_REASON rst_reas[SOC_CPU_CORES_NUM];
#else
    RESET_REASON rst_reas[1];
#endif

    bootloader_init_mem();

    // Move exception vectors to IRAM
    cpu_hal_set_vecbase(&_init_start);

    rst_reas[0] = rtc_get_reset_reason(0);
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    rst_reas[1] = rtc_get_reset_reason(1);
#endif

#ifndef CONFIG_BOOTLOADER_WDT_ENABLE
    // from panic handler we can be reset by RWDT or TG0WDT
    if (rst_reas[0] == RTCWDT_SYS_RESET || rst_reas[0] == TG0WDT_SYS_RESET
#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
        || rst_reas[1] == RTCWDT_SYS_RESET || rst_reas[1] == TG0WDT_SYS_RESET
#endif
    ) {
#ifndef CONFIG_BOOTLOADER_WDT_ENABLE
        wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_disable(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
#endif
    }
#endif

    //Clear BSS. Please do not attempt to do any complex stuff (like early logging) before this.
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_ESP32_IRAM_AS_8BIT_ACCESSIBLE_MEMORY)
    // Clear IRAM BSS
    memset(&_iram_bss_start, 0, (&_iram_bss_end - &_iram_bss_start) * sizeof(_iram_bss_start));
#endif

    /* Unless waking from deep sleep (implying RTC memory is intact), clear RTC bss */
    if (rst_reas[0] != DEEPSLEEP_RESET) {
        memset(&_rtc_bss_start, 0, (&_rtc_bss_end - &_rtc_bss_start) * sizeof(_rtc_bss_start));
    }

#if CONFIG_IDF_TARGET_ESP32S2
    /* Configure the mode of instruction cache : cache size, cache associated ways, cache line size. */
    extern void esp_config_instruction_cache_mode(void);
    esp_config_instruction_cache_mode();

    /* If we need use SPIRAM, we should use data cache, or if we want to access rodata, we also should use data cache.
       Configure the mode of data : cache size, cache associated ways, cache line size.
       Enable data cache, so if we don't use SPIRAM, it just works. */
#if CONFIG_SPIRAM_BOOT_INIT
    extern void esp_config_data_cache_mode(void);
    esp_config_data_cache_mode();
    Cache_Enable_DCache(0);
#endif
#endif

#if CONFIG_SPIRAM_BOOT_INIT
    esp_spiram_init_cache();
    if (esp_spiram_init() != ESP_OK) {
#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
        ESP_EARLY_LOGE(TAG, "Failed to init external RAM, needed for external .bss segment");
        abort();
#endif
#endif

#if CONFIG_SPIRAM_IGNORE_NOTFOUND
        ESP_EARLY_LOGI(TAG, "Failed to init external RAM; continuing without it.");
        g_spiram_ok = false;
#else
        ESP_EARLY_LOGE(TAG, "Failed to init external RAM!");
        abort();
#endif
    }
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    s_cpu_up[0] = true;
#endif
    ESP_EARLY_LOGI(TAG, "Pro cpu up.");

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    start_other_core();
#endif

#if CONFIG_SPIRAM_MEMTEST
    if (g_spiram_ok) {
        bool ext_ram_ok = esp_spiram_test();
        if (!ext_ram_ok) {
            ESP_EARLY_LOGE(TAG, "External RAM failed memory test!");
            abort();
        }
    }
#endif

#if CONFIG_IDF_TARGET_ESP32S2
#if CONFIG_SPIRAM_FETCH_INSTRUCTIONS
    extern void instruction_flash_page_info_init(void);
    instruction_flash_page_info_init();
#endif
#if CONFIG_SPIRAM_RODATA
    extern void rodata_flash_page_info_init(void);
    rodata_flash_page_info_init();
#endif

#if CONFIG_SPIRAM_FETCH_INSTRUCTIONS
    extern void esp_spiram_enable_instruction_access(void);
    esp_spiram_enable_instruction_access();
#endif
#if CONFIG_SPIRAM_RODATA
    extern void esp_spiram_enable_rodata_access(void);
    esp_spiram_enable_rodata_access();
#endif

#if CONFIG_ESP32S2_INSTRUCTION_CACHE_WRAP || CONFIG_ESP32S2_DATA_CACHE_WRAP
    uint32_t icache_wrap_enable = 0, dcache_wrap_enable = 0;
#if CONFIG_ESP32S2_INSTRUCTION_CACHE_WRAP
    icache_wrap_enable = 1;
#endif
#if CONFIG_ESP32S2_DATA_CACHE_WRAP
    dcache_wrap_enable = 1;
#endif
    extern void esp_enable_cache_wrap(uint32_t icache_wrap_enable, uint32_t dcache_wrap_enable);
    esp_enable_cache_wrap(icache_wrap_enable, dcache_wrap_enable);
#endif
#endif // CONFIG_IDF_TARGET_ESP32S2

#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
    memset(&_ext_ram_bss_start, 0, (&_ext_ram_bss_end - &_ext_ram_bss_start) * sizeof(_ext_ram_bss_start));
#endif

//Enable trace memory and immediately start trace.
#if CONFIG_ESP32_TRAX || CONFIG_ESP32S2_TRAX
#if CONFIG_IDF_TARGET_ESP32
    #if CONFIG_ESP32_TRAX_TWOBANKS
        trax_enable(TRAX_ENA_PRO_APP);
    #else 
        trax_enable(TRAX_ENA_PRO);
    #endif
#elif CONFIG_IDF_TARGET_ESP32S2 
    trax_enable(TRAX_ENA_PRO);
#endif
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif // CONFIG_ESP32_TRAX || CONFIG_ESP32S2_TRAX

    esp_clk_init();
    esp_perip_clk_init();
    intr_matrix_clear();

#if CONFIG_ESP32_BROWNOUT_DET || CONFIG_ESP32S2_BROWNOUT_DET
    esp_brownout_init();
#endif

    rtc_gpio_force_hold_dis_all();

    esp_cache_err_int_init();

#if CONFIG_IDF_TARGET_ESP32S2
#if CONFIG_ESP32S2_MEMPROT_FEATURE
#if CONFIG_ESP32S2_MEMPROT_FEATURE_LOCK
    esp_memprot_set_prot(true, true);
#else
    esp_memprot_set_prot(true, false);
#endif
#endif
#endif

    bootloader_flash_update_id();
#if CONFIG_IDF_TARGET_ESP32 
#if !CONFIG_SPIRAM_BOOT_INIT
    // Read the application binary image header. This will also decrypt the header if the image is encrypted.
    esp_image_header_t fhdr = {0};
#ifdef CONFIG_APP_BUILD_TYPE_ELF_RAM
    fhdr.spi_mode = ESP_IMAGE_SPI_MODE_DIO;
    fhdr.spi_speed = ESP_IMAGE_SPI_SPEED_40M;
    fhdr.spi_size = ESP_IMAGE_FLASH_SIZE_4MB;

    extern void esp_rom_spiflash_attach(uint32_t, bool);
    esp_rom_spiflash_attach(ets_efuse_get_spiconfig(), false);
    esp_rom_spiflash_unlock();
#else
    // This assumes that DROM is the first segment in the application binary, i.e. that we can read
    // the binary header through cache by accessing SOC_DROM_LOW address.
    memcpy(&fhdr, (void*) SOC_DROM_LOW, sizeof(fhdr));
#endif // CONFIG_APP_BUILD_TYPE_ELF_RAM

    // If psram is uninitialized, we need to improve some flash configuration.
    bootloader_flash_clock_config(&fhdr);
    bootloader_flash_gpio_config(&fhdr);
    bootloader_flash_dummy_config(&fhdr);
    bootloader_flash_cs_timing_config();
#endif //!CONFIG_SPIRAM_BOOT_INIT
#endif

#if !CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    s_cpu_inited[0] = true;

    volatile bool cpus_inited = false;

    while(!cpus_inited) {
        cpus_inited = true;
        for (int i = 0; i < SOC_CPU_CORES_NUM; i++) {
            cpus_inited &= s_cpu_inited[i];
        }
        cpu_hal_delay_us(100);
    }
#endif

    SYS_STARTUP_FN();
}