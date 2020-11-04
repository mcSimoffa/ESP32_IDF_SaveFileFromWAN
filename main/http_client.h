
#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

typedef enum
{
	TIMEOUT_PACKET	= 0,
	NOTIFY,
	LAST_PACKET_NOTIFY,
	WRONG_PACKET_NOTIFY,
	NOT_PERMITT
} eNotyfy_Result;

void network_task (void *pvParameters);

eNotyfy_Result Get_http_data(uint8_t *dst, int *len);

#endif
