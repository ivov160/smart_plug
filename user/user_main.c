#include "user_config.h"

#include "esp_common.h"

#ifdef NDEBUG
#include "../esp-gdbstub/gdbstub.h"
#endif

#include "uart.h"

#include "wifi_station.h"
#include "user_power.h"
#include "user_http_handlers.h"

#include "light_http.h"
#include "flash.h"


LOCAL os_timer_t info_timer;

static struct http_handler_rule handlers[] = 
{	
	{ "/getSystemInfo", http_system_info_handler },
	{ "/getDeviceInfo", http_get_device_info_handler },
	/*{ "/getBroadcastNetworks", http_get_wifi_info_list_handler },*/
	/*{ "/setDeviceName", http_set_device_name_handler },*/
	{ "/on", http_on_handler },
	{ "/off", http_off_handler },
	{ "/status", http_status_handler },
	{ NULL, NULL },
};

LOCAL void system_info(void *p_args)
{
	os_printf("system: SDK version:%s rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check());
	os_printf("system: Chip id = 0x%x\r\n", system_get_chip_id());
	os_printf("system: CPU freq = %d MHz\r\n", system_get_cpu_freq());
	os_printf("system: Free heap size = %d\r\n", system_get_free_heap_size());
}

LOCAL void scan_callback(void *args, STATUS status)
{
	if(args != NULL)
	{
		struct bss_info* bss_list = (struct bss_info*) args;
		struct bss_info *bss = bss_list;

		uint32_t wifi_index = 0;
		while(bss != NULL && wifi_index < WIFI_LIST_SIZE)
		{
			/*struct wifi_info info;*/
			/*memset(&info, 0, sizeof(struct wifi_info));*/

			os_printf("wifi: scaned ssid: `%s`\n", bss->ssid);

			/*if(bss->ssid_len < WIFI_NAME_SIZE - 1)*/
			/*{*/
				/*memcpy(info.name, bss->ssid, bss->ssid_len);*/
				/*// явный нолик*/
				/*info.name[WIFI_NAME_SIZE - 1] = 0;*/

				/*if(!write_wifi_info(&info, wifi_index))*/
				/*{*/
					/*os_printf("wifi: failed save wifi settings by index: %d\n", wifi_index);*/
				/*}*/
				/*else*/
				/*{*/
					/*++wifi_index;*/
				/*}*/
			/*}*/
			/*else*/
			/*{*/
				/*os_printf("wifi: ssid is too long ssid: `%s`\n", bss->ssid);*/
			/*}*/
			bss = STAILQ_NEXT(bss, next);
		}
		free(args);
	}
	else
	{
		os_printf("wifi: scan results arg is NULL\n");
	}
}

LOCAL void scan_wifi()
{
	struct scan_config scan;
	while(!wifi_station_scan(&scan, scan_callback))
	{
		os_printf("wifi: failed start scan stations\n");
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

LOCAL void main_task(void *pvParameters)
{
	scan_wifi();
	while(true)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
    vTaskDelete(NULL);
}


void user_init(void)
{
	/*uint32_t start_time = system_get_rtc_time();*/
	uint32_t start_time = system_get_time();

	uart_init_new();
	UART_SetBaudrate(UART0, BIT_RATE_115200);

#ifdef NDEBUG
	// компилируем если с отладкой
	gdbstub_init();
#endif

	power_init();
	power_up();

	os_timer_setfn(&info_timer, system_info, NULL);
	os_timer_arm(&info_timer, 4000, true);

	init_layout();

	///@todo read about task memory
	xTaskCreate(main_task, "main_task", 280, NULL, MAIN_TASK_PRIO, NULL);


	/*struct custom_name name_info;*/
	/*memset(&name_info, 0, sizeof(struct custom_name));*/
	/*sprintf(name_info.data, "TEST NAME SUKA");*/

	/*if(!write_custom_name(&name_info))*/
	/*{*/
		/*os_printf("test: can't write custom_name\n");*/
	/*}*/

	/*if(spi_flash_erase_sector(FLASH_BASE_ADDR / SPI_FLASH_SEC_SIZE) != SPI_FLASH_RESULT_OK)*/
	/*{*/
		/*os_printf("test: failed erase sector 108\n");*/
	/*}*/

	/*char* d = NULL;*/
	/**d = '\0';*/

	struct device_info info;
	memset(&info, 0, sizeof(struct device_info));
	read_current_device(&info);

	start_wifi(&info);
	webserver_start(handlers);


	/*uint32_t end_time = system_get_rtc_time();*/
	uint32_t end_time = system_get_time();
	/*os_printf("time: system up by: %umks\n", (end_time - start_time)*system_rtc_clock_cali_proc());*/
	os_printf("time: system up by: %umks\n", (end_time - start_time));
}

