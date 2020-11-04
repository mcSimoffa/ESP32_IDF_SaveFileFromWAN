/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "esp_http_client.h"

#include "http_client.h"
#include "wifi_connect.h"

#define URL_TO_LOAD						CONFIG_URLFILE
#define MAX_HTTP_RECV_BUFFER 			CONFIG_EXCHANG_BUF_SIZE
#define NOTIFY_TIMEOUT_TICKS			1000	// 10 sec

extern TaskHandle_t store_task_h;
extern TaskHandle_t network_task_h;

static const char *TAG = "HTTP_CLIENT";

char fileReadBuf[MAX_HTTP_RECV_BUFFER + 1];
int rdlen = 0;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client))
            {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0)
            {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

static void http_stream_read()
{
	esp_err_t errcode;
	int quant_length;
	uint8_t abnormalFlag;
	eNotyfy_Result notify;

	esp_http_client_config_t config =
	    {
	        .url = URL_TO_LOAD,
	        .event_handler = _http_event_handler,
	    };
	esp_http_client_handle_t client = esp_http_client_init(&config);

	errcode = esp_http_client_open(client, 0);	//only for reading
	if (errcode)
	{
		ESP_LOGE(TAG, "%s Open failed: %s", URL_TO_LOAD, esp_err_to_name(errcode));
		esp_http_client_cleanup(client);
		return;
	}
	ESP_LOGI (TAG, "Successful open %s", URL_TO_LOAD);

	int content_length =  esp_http_client_fetch_headers(client);
	ESP_LOGI (TAG, "Content length %d bytes", content_length);
	abnormalFlag = 0;
	while (content_length > 0)
	{
		ESP_LOGI (TAG, "Waiting permitting from store_task.....");
		if (ulTaskNotifyTake(pdFALSE, NOTIFY_TIMEOUT_TICKS) == TIMEOUT_PACKET)
		{
			ESP_LOGE(TAG, "Timeout permitting from store_task");
			abnormalFlag = 1;
			rdlen = 0;
			content_length = 0;
			notify = NOT_PERMITT;
			break;
		}
		ESP_LOGI(TAG, "Have permitting");

		quant_length = (content_length > MAX_HTTP_RECV_BUFFER) ? MAX_HTTP_RECV_BUFFER : content_length;
		rdlen = esp_http_client_read (client, fileReadBuf, quant_length );
		ESP_LOGI (TAG, "Download %d bytes", rdlen);
		if (rdlen == -1)
		{
			ESP_LOGE(TAG, "Client Read error");
			abnormalFlag = 1;
			rdlen = 0;
			content_length = 0;
			notify = WRONG_PACKET_NOTIFY;
			break;
		}
		else
		{
			content_length -= rdlen;
			notify = (content_length) ? NOTIFY : LAST_PACKET_NOTIFY;
		}
		//send notification for store_task about: "rdlen bytes is ready for save on SD card"
		xTaskNotify (store_task_h, notify, eSetValueWithOverwrite);
		ESP_LOGI (TAG, "Remaining %d bytes", content_length);
	}	//while (content_length > 0)

	if (abnormalFlag)
	{
		xTaskNotify (store_task_h, notify, eSetValueWithOverwrite);
		abnormalFlag = 0;
	}

	ESP_LOGI (TAG, "Close connect");
	esp_http_client_close(client);

}

/* **************************************************************************
 * Blocking routine for getting data from HTTP client
 * dst - pointer for data store. Array must be more than MAX_HTTP_RECV_BUFFER
 * len - pointer for pass length of getting data
 * return:
 * 			TIMEOUT_PACKET 		- if HTTP client don't pass data and time expire
 * 			LAST_PACKET_NOTIFY	- if it's last data packet
 * 			NOTIFY				- if isn't last data packet
 ************************************************************************* */
eNotyfy_Result Get_http_data(uint8_t *dst, int *len)
{
	*len = 0;
	//wait a new dataArray
	uint8_t waitResult = ulTaskNotifyTake(pdFALSE, NOTIFY_TIMEOUT_TICKS);
	if (waitResult == NOTIFY || waitResult == LAST_PACKET_NOTIFY)
	{
		*len = rdlen;
		ESP_LOGI(TAG, "Incoming notification about %d bytes", *len);
		if (*len > 0)
		{
			memcpy(dst, fileReadBuf, *len);
			//this is send notification for network_task about: "I saved yet this array, you may read still one more"
			xTaskNotify(network_task_h, NOTIFY, eSetValueWithOverwrite);
		}
	}
	return (waitResult);
}



void network_task (void *pvParameters)
{
	wifi_init_sta();
	http_stream_read();
    ESP_LOGI(TAG, "Application finish");
    while(1)
    {
		vTaskDelay(500);
		ESP_LOGI(TAG, "Cycle Delay");
    }
}

