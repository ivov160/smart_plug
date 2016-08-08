#include "wifi_station.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_common.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define MAX_AP_CONNECTION 10
#define PASSWORD_LENGTH 8
#define MAX_SSID_LENGTH 32

LOCAL struct softap_config* ap_config = NULL;

LOCAL char int_to_hex[] = "0123456789ABCDEF";

void generate_password(char* password, uint32_t size)
{
	uint8_t current_size = 0;
	while(current_size < size)
	{
		unsigned long number = os_random();
		uint8_t value = number % 16;
		
		password[current_size] = int_to_hex[value];
		++current_size;
	}
}

void start_wifi(struct device_info* info)
{
	if(ap_config == NULL)
	{
		const char* device_type_str = device_info_get_type(info);
		char ssid[MAX_SSID_LENGTH] = { 0 };
		char password[PASSWORD_LENGTH + 1] = { 0 };

		generate_password(password, PASSWORD_LENGTH);

		int render_size = sprintf(ssid, "LOOKin_%s_%s", device_type_str, password);
		if(render_size < 0)
		{
			os_printf("wifi: failed generate ssid\n");
			return;
		}

		ap_config = (struct softap_config *)zalloc(sizeof(struct softap_config));

		memcpy(ap_config->ssid, ssid, render_size);
		ap_config->ssid_len = render_size;

		memcpy(ap_config->password, password, PASSWORD_LENGTH + 1);

		ap_config->channel = 1;
		ap_config->authmode = AUTH_WPA2_PSK;
		ap_config->ssid_hidden = 0;

		ap_config->max_connection = MAX_AP_CONNECTION;

		os_printf("wifi: ssid: %s, render_size: %d, password: %s\n", ap_config->ssid, render_size, ap_config->password);
		if(!wifi_softap_set_config_current(ap_config))
		{
			os_printf("wifi: failed set softap_config\n");
			return;
		}
	}

	if(!wifi_set_opmode_current(STATIONAP_MODE))
	{
		os_printf("wifi: failed initialize STATIONAP_MODE\n");
		return;
	}

	if(!wifi_softap_dhcps_start())
	{
		os_printf("wifi: failed start dhcp server\n");
		return;
	}
}

void stop_wifi()
{
	if(ap_config != NULL)
	{
		if(DHCP_STARTED == wifi_softap_dhcps_status() && !wifi_softap_dhcps_stop())
		{
			os_printf("wifi: failed stop dhcp server\n");
		}

		if(!wifi_set_opmode_current(NULL_MODE))
		{
			os_printf("wifi: failed disable wifi\n");
		}

		free(ap_config);
		ap_config = NULL;
	}
}
