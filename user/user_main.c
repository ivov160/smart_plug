#include "user_config.h"

#include "esp_common.h"

#ifdef NDEBUG
#include "../esp-gdbstub/gdbstub.h"
#endif

#include "uart.h"

#include "wifi_station.h"
#include "user_power.h"
#include "user_http_handlers.h"

#include "asio_light_http.h"
#include "flash.h"


LOCAL os_timer_t info_timer;

static struct http_handler_rule handlers[] = 
{	
	{ "/getSystemInfo", http_system_info_handler },
	{ "/getDeviceInfo", http_get_device_info_handler },
	/*{ "/getBroadcastNetworks", http_get_wifi_info_list_handler },*/
	{ "/getBroadcastNetworks", http_scan_wifi_info_list_handler },
	{ "/setDeviceName", http_set_device_name_handler },
	{ "/setWifi", http_set_main_wifi_handler },
	{ "/getWifiError", http_get_wifi_error_handler },
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

void user_init(void)
{
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

	struct wifi_info main_wifi;
	memset(&main_wifi, 0, sizeof(struct wifi_info));
	if(!read_main_wifi(&main_wifi))
	{
		os_printf("user: failed get main_wifi\n");
	}

	uint32_t name_length = strnlen(main_wifi.name, WIFI_NAME_SIZE);
	if(name_length != 0 && name_length < WIFI_NAME_SIZE)
	{
		start_station_wifi(&main_wifi, false);
	}
	else
	{
		struct device_info info;
		memset(&info, 0, sizeof(struct device_info));
		if(!read_current_device(&info))
		{
			os_printf("user: failed get current device_info\n");
		}
		start_ap_wifi(&info);
	}
	asio_webserver_start(handlers);

	uint32_t end_time = system_get_time();
	os_printf("time: system up by: %umks\n", (end_time - start_time));
}

