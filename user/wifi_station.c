#include "wifi_station.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_common.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define MAX_AP_CONNECTION 10

/*LOCAL struct station_config *sta_config = NULL;*/
LOCAL struct softap_config* ap_config = NULL;

void start_wifi()
{
	if(ap_config == NULL)
	{
		ap_config = (struct softap_config *)zalloc(sizeof(struct softap_config));
		/*if(!wifi_softap_get_config(ap_config))*/
		/*{*/
			/*os_printf("failed getting softap_config\n");*/
			/*return;*/
		/*}*/

		char ssid[10] = { 0 };
		char password[10] = { 0 };

		sprintf(ssid, "ESP_WIFI");
		sprintf(password, "BmW24zQ5");

		memcpy(ap_config->ssid, ssid, strlen(ssid));
		ap_config->ssid_len = strlen(ssid);

		memcpy(ap_config->password, password, strlen(password));

		ap_config->channel = 1;
		ap_config->authmode = AUTH_WPA2_PSK;
		ap_config->ssid_hidden = 0;

		ap_config->max_connection = MAX_AP_CONNECTION;

		if(!wifi_softap_set_config(ap_config))
		{
			os_printf("failed set softap_config\n");
			return;
		}
	}

	if(!wifi_set_opmode(STATIONAP_MODE))
	{
		os_printf("failed initialize STATIONAP_MODE\n");
		return;
	}

	if(!wifi_softap_dhcps_start())
	{
		os_printf("failed start dhcp server\n");
		return;
	}
}
