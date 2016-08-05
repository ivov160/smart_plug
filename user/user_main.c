#include "user_config.h"

#include "esp_common.h"
#include "../esp-gdbstub/gdbstub.h"

#include "uart.h"
#include "wifi_station.h"
#include "spiffs/spiffs.h"

#include "user_http_handlers.h"
#include "light_http.h"
#include "flash.h"


LOCAL os_timer_t info_timer;

static struct http_handler_rule handlers[] = 
{	
	{ "/getSystemInfo", http_system_info_handler },
	{ "/getDeviceInfo", http_get_device_info_handler },
	{ "/getBroadcastNetworks", http_get_wifi_info_list_handler },
	{ "/setDeviceName", http_set_device_name_handler },
	{ NULL, NULL },
};

LOCAL void ICACHE_FLASH_ATTR system_info(void *p_args)
{
	os_printf("system: SDK version:%s rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check());
	os_printf("system: Chip id = 0x%x\r\n", system_get_chip_id());
	os_printf("system: CPU freq = %d MHz\r\n", system_get_cpu_freq());
	os_printf("system: Free heap size = %d\r\n", system_get_free_heap_size());
}

void ICACHE_FLASH_ATTR scan_callback(void *args, STATUS status)
{
	if(args != NULL)
	{
		struct bss_info* bss_list = (struct bss_info*) args;
		struct bss_info *bss = bss_list;

		uint32_t wifi_index = 0;
		while(bss != NULL && wifi_index < WIFI_LIST_SIZE)
		{
			struct wifi_info info;
			memset(&info, 0, sizeof(struct wifi_info));

			os_printf("wifi: scaned ssid: `%s` size: %d index: %d\n", bss->ssid, bss->ssid_len, wifi_index);

			// do that because bss->ssid_len 0
			uint8_t ssid_len = strnlen(bss->ssid, WIFI_NAME_SIZE);
			if(ssid_len< WIFI_NAME_SIZE - 1)
			{
				memcpy(info.name, bss->ssid, ssid_len);
				if(!write_wifi_info(&info, wifi_index))
				{
					os_printf("wifi: failed save wifi settings by index: %d\n", wifi_index);
				}
				else
				{
					++wifi_index;
				}
			}
			else
			{
				os_printf("wifi: ssid is too long ssid: `%s`\n", bss->ssid);
			}
			bss = STAILQ_NEXT(bss, next);
		}
		free(args);
	}
	else
	{
		os_printf("wifi: scan results arg is NULL\n");
	}
}

LOCAL void ICACHE_FLASH_ATTR scan_wifi()
{
	/*struct scan_config scan;*/
	/*while(!wifi_station_scan(&scan, scan_callback))*/
	/*{*/
		/*os_printf("wifi: failed start scan stations\n");*/
		/*vTaskDelay(1000 / portTICK_RATE_MS);*/
	/*}*/

	struct custom_name n;
	memset(&n, 0, sizeof(struct custom_name));
	memcpy(n.data, "42", sizeof("42") - 1);

	if(!write_custom_name(&n))
	{
		os_printf("wifi: failed write custom_name: `%s`\n", n.data);
	}

	char* d = NULL;
	*d = 'd';

	memset(&n, 0, sizeof(struct custom_name));
	memcpy(n.data, "33", sizeof("33") - 1);

	if(!write_custom_name(&n))
	{
		os_printf("wifi: failed write custom_name: `%s`\n", n.data);
	}
}

LOCAL void ICACHE_FLASH_ATTR main_task(void *pvParameters)
{
	/*scan_wifi();*/
	while(true)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
    vTaskDelete(NULL);
}

void user_init(void)
{
	uart_init_new();
	UART_SetBaudrate(UART0, BIT_RATE_115200);

	gdbstub_init();

	os_timer_setfn(&info_timer, system_info, NULL);
	os_timer_arm(&info_timer, 4000, true);

	///@todo read about task memory
	/*xTaskCreate(main_task, "main_task", 280, NULL, 4, NULL);*/

	start_wifi();
	webserver_start(handlers);
}

