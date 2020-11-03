
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
//#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
//#include "lwip/err.h"
//#include "lwip/sys.h"

#include "http_client.h"
#include "sd_card.h"

BaseType_t network_task_h = 0;
BaseType_t store_task_h = 0;

void app_main(void)
{
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

	network_task_h 	= xTaskCreate (&network_task, "network_task", 8192, NULL, 5, NULL);
	xTaskNotify(network_task, 1, eSetValueWithOverwrite);	//unblock http_read
	store_task_h 	= xTaskCreate (&store_task, "store_task", 8192, NULL, 5, NULL);

    while(1) {}
}
