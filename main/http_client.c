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

#define URL_TO_LOAD						"https://www.sgo.fi/Data/RealTime/Kuvat/magrtday.png"	//CONFIG_URLFILE
#define MAX_HTTP_RECV_BUFFER 			512
#define NOTIFY_TIMEOUT_TICKS			1000

extern TaskHandle_t store_task_h;
extern TaskHandle_t network_task_h;

static const char *TAG = "HTTP_CLIENT";


char fileReadBuf[MAX_HTTP_RECV_BUFFER + 1];
/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
//extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
//extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client))
            {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
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
	int rdlen = 0;
	//BaseType_t retVal;
	//uint32_t store_task_notif;
	esp_http_client_config_t config =
	    {
	        .url = URL_TO_LOAD,
	        .event_handler = _http_event_handler,
	    };
	esp_http_client_handle_t client = esp_http_client_init(&config);

	errcode = esp_http_client_open(client, 0);
	if (errcode)
	{
		ESP_LOGE(TAG, "Client Open failed: %s", esp_err_to_name(errcode));
		return;
	}
	ESP_LOGI (TAG, "Client was open %s", URL_TO_LOAD);

	int content_length =  esp_http_client_fetch_headers(client);
	ESP_LOGI (TAG, "Content length %d bytes", content_length);
	int quant_length = (content_length > MAX_HTTP_RECV_BUFFER) ? MAX_HTTP_RECV_BUFFER : content_length;

	while (quant_length > 0)
	{
		ESP_LOGI (TAG, "Waiting for SDcard ready..");
		if (ulTaskNotifyTake(pdFALSE, NOTIFY_TIMEOUT_TICKS) == 0)
		{
			ESP_LOGE(TAG, "Timeout waiting SDcard");
			break;
		}
		ESP_LOGI(TAG, "Have permitting for http_read");

		rdlen = esp_http_client_read (client, fileReadBuf, quant_length );
		ESP_LOGI (TAG, "Downloaded %d bytes", rdlen);
		if (rdlen == -1)
		{
			ESP_LOGE(TAG, "Client Read error");
			break;
		}
		//printf("%.*s", rdlen, (char*)fileReadBuf);
		quant_length -= rdlen;

		//this is send notification for store_task about: "rdlen bytes is ready for save on SD card"
		if (xTaskNotify (store_task_h, (uint32_t)rdlen, eSetValueWithoutOverwrite) == pdFALSE)
		{
			ESP_LOGE(TAG, "Store_task notify error");
			break;
		}
		ESP_LOGI (TAG, "Remaining %d bytes", quant_length);
	}

	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

uint32_t Get_http_data(uint8_t *dst)
{
	//wait a new dataArray
	uint32_t rdlen_notif = ulTaskNotifyTake(pdFALSE, NOTIFY_TIMEOUT_TICKS);	//wait binary
	ESP_LOGI(TAG, "Incoming notification about %d bytes", rdlen_notif);
	if (rdlen_notif >0)
	{
		memcpy(dst, fileReadBuf, rdlen_notif);
		//this is send notification for network_task about: "I saved yet this array, you may read still one more"
		if (xTaskNotify(network_task_h, 1, eSetValueWithoutOverwrite) == pdFALSE)
		{
			ESP_LOGE(TAG, "network_task notify error");
		}
	}
	return (rdlen_notif);
}


static void http_perform_as_stream_reader(void)
{
    char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc http receive buffer");
        return;
    }
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(buffer);
        return;
    }
    int content_length =  esp_http_client_fetch_headers(client);
    int total_read_len = 0, read_len;
    if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER)
    {
        read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len <= 0) {
            ESP_LOGE(TAG, "Error read data");
        }
        buffer[read_len] = 0;
        ESP_LOGD(TAG, "read_len = %d", read_len);
    }
    ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}


void network_task (void *pvParameters)
{
	wifi_init_sta();
	http_stream_read();
    //http_perform_as_stream_reader();
    ESP_LOGI(TAG, "Application finish");
    while(1)
    {
		vTaskDelay(100);
		ESP_LOGI(TAG, " cycle Delay");
    }
}

