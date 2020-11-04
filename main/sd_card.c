/* This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "http_client.h"

#define FILEPATHNAME	CONFIG_FILEPATHNAME
#define EXCHANGEBUFSIZE	CONFIG_EXCHANG_BUF_SIZE

static const char *TAG = "SD_CARD";
uint8_t writeBuf[EXCHANGEBUFSIZE + 1];

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

// #define USE_SPI_MODE

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13
#endif //USE_SPI_MODE

void store_task (void *pvParameters)
{
	int getlen = 0;
	eNotyfy_Result nRes;
    ESP_LOGI(TAG, "Initializing SD card");

#ifndef USE_SPI_MODE
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;

    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

#else
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif //USE_SPI_MODE

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config =
    {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    do
    {	//while (0)
		if (ret != ESP_OK)
		{
			if (ret == ESP_FAIL)
			{
				ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
			}
			else
			{
				ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
			}
			break;
		}

		// Card has been initialized, print its properties
		sdmmc_card_print_info(stdout, card);

		// Use POSIX and C standard library functions to work with files.
		// First create a file.
		ESP_LOGI(TAG, "Opening for Write");
		FILE* f = fopen(FILEPATHNAME, "w");
		if (f == NULL)
		{
			ESP_LOGE(TAG, "Failed to open file for writing");
			break;
		}

		//cycle for part by part file save
		do
		{
			nRes = Get_http_data(writeBuf, &getlen);
			if (nRes == NOTIFY || nRes == LAST_PACKET_NOTIFY)
			{
				fwrite(writeBuf, 1, getlen, f);
				ESP_LOGI(TAG, "write %d bytes", getlen);
			}
		} while (nRes == NOTIFY);

		fclose(f);
		if (nRes == LAST_PACKET_NOTIFY)
			ESP_LOGI(TAG, "File written successful");
		else
		{
			ESP_LOGI(TAG, "File write error and will be deleting");
			unlink(FILEPATHNAME);
		}
    } while (0);

	// Unmount partition and disable SDMMC or SPI peripheral
	esp_vfs_fat_sdmmc_unmount();
	ESP_LOGI(TAG, "Card unmounted");

    while (1)
    {
    	vTaskDelay(500);
    	ESP_LOGI(TAG, " cycle Delay");
    }
}
