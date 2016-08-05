#include "wifi_station.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_common.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "flash.h"

#define MAX_AP_CONNECTION 10
#define PASSWORD_LENGTH 6
#define MAX_SSID_LENGTH 32

LOCAL struct softap_config* ap_config = NULL;

LOCAL char int_to_hex[] = "0123456789ABCDEF";

void ICACHE_FLASH_ATTR generate_password(char* password, uint32_t size)
{
	uint8_t current_size = 0;
	while(current_size < size)
	{
		unsigned long number = os_random();
		uint8_t value = number % 16;
		
		/*if(strchar(password, value) == NULL)*/
		/*{*/
			password[current_size] = int_to_hex[value];
			++current_size;
		/*}*/
	}
}

void ICACHE_FLASH_ATTR start_wifi()
{
	if(ap_config == NULL)
	{
		/*struct device_info device_info;*/
		/*memset(&device_info, 0, sizeof(struct device_info));*/

		/*if(!read_current_device(&device_info))*/
		/*{*/
			/*os_printf("wifi: failed get current device info\n");*/
			/*return;*/
		/*}*/

		/*const char* device_type_str = device_info_get_type(&device_info);*/
		char device_type_str[] = "PLUG";
		char ssid[MAX_SSID_LENGTH] = { 0 };
		char password[PASSWORD_LENGTH + 1] = { 0 };

		generate_password(password, PASSWORD_LENGTH);

		int render_size = sprintf(ssid, "LOOKin_%s_%s", device_type_str, password);
		/*int render_size = sprintf(ssid, "ESP_WIFI");*/
		if(render_size < 0)
		{
			os_printf("wifi: failed generate ssid\n");
			return;
		}

		ap_config = (struct softap_config *)zalloc(sizeof(struct softap_config));
		if(!wifi_softap_get_config(ap_config))
		{
			os_printf("failed getting softap_config\n");
			return;
		}

		memcpy(ap_config->ssid, ssid, strlen(ssid));
		ap_config->ssid_len = strlen(ssid);

		memcpy(ap_config->password, password, strlen(password));

		ap_config->channel = 1;
		ap_config->authmode = AUTH_WPA2_PSK;
		ap_config->ssid_hidden = 0;

		ap_config->max_connection = MAX_AP_CONNECTION;

		os_printf("wifi: ssid: %s, render_size: %d, password: %s\n", ap_config->ssid, render_size, ap_config->password);

		/*if(!wifi_softap_set_config(ap_config) || !wifi_softap_set_config_current(ap_config))*/
		/*if(!wifi_softap_set_config_current(ap_config))*/
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
