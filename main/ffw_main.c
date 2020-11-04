
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "http_client.h"
#include "sd_card.h"

TaskHandle_t network_task_h = NULL;
TaskHandle_t store_task_h = NULL;

static const char *TAG = "MAIN";

void app_main(void)
{
	ESP_LOGI(TAG, "\r\n\nLaunching");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // configure tasks
    do
    {
		if (xTaskCreate (&network_task, "network_task", 8192, NULL, 5, &network_task_h) != pdTRUE)
		{
			ESP_LOGE(TAG, "Network task failed to launch");
			break;
		}

		//unblock http_read
		if (xTaskNotify(network_task_h, NOTIFY, eSetValueWithOverwrite) != pdPASS)
		{
			ESP_LOGE(TAG, "Notify for unlock network task failed to send");
			break;
		}

		if (xTaskCreate (&store_task, "store_task", 8192, NULL, 5, &store_task_h) !=pdPASS)
		{
			ESP_LOGE(TAG, "Store task failed to launch");
			break;
		}

		ESP_LOGI(TAG, "Tasks successfully launched");
    } while (0);

    //main cycle there is empty
    while(1)
    {
    	vTaskDelay(500);
    	ESP_LOGI(TAG, " cycle Delay");
    }
}
