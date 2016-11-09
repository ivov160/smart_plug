#include "user_config.h"

#include "esp_common.h"

#ifdef NDEBUG
#include "../esp-gdbstub/gdbstub.h"
#endif

#include "uart.h"

#include "user_wifi.h"
#include "user_power.h"
#include "user_http_handlers.h"
#include "user_mesh_handlers.h"

#include "../mesh/mesh.h"
#include "../data/data.h"
#include "../light_http/light_http.h"


LOCAL os_timer_t info_timer;

static struct http_handler_rule http_handlers[] = 
{	
	{ "/getSystemInfo", http_system_info_handler },
	{ "/getDeviceInfo", http_get_device_info_handler },
	{ "/getBroadcastNetworks", http_scan_wifi_info_list_handler },
	{ "/setDeviceName", http_set_device_name_handler },
	{ "/setWifi", http_set_main_wifi_handler },
	{ "/getWifiError", http_get_wifi_error_handler },
	{ "/on", http_on_handler },
	{ "/off", http_off_handler },
	{ "/status", http_status_handler },
	{ "/testModeOn",  http_start_test_mode},
	{ "/testModeOff",  http_stop_test_mode},
	{ NULL, NULL },
};

static struct mesh_message_handlers mesh_handlers[] = 
{	
	{ mesh_keep_alive, mesh_keep_alive_handler },
	{ mesh_devices_info_request, mesh_devices_info_request_handler },
	{ mesh_device_info_response, mesh_device_info_response_handler },
	{ mesh_device_info_response_confirm, mesh_device_info_response_confirm_handler },
	{ mesh_keep_alive, NULL },
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

	data_init_layout();

	struct data_wifi_info main_wifi;
	memset(&main_wifi, 0, sizeof(struct data_wifi_info));
	if(!data_read_main_wifi(&main_wifi))
	{
		os_printf("user: failed get main_wifi\n");
	}

	uint32_t name_length = strnlen(main_wifi.name, DATA_WIFI_NAME_SIZE);
	if(name_length != 0 && name_length < DATA_WIFI_NAME_SIZE)
	{
		wifi_start_station(&main_wifi, false);
	}
	else
	{
		struct data_device_info info;
		memset(&info, 0, sizeof(struct data_device_info));
		if(!data_read_current_device(&info))
		{
			os_printf("user: failed get current device_info\n");
		}
		wifi_start_ap(&info);
	}
	asio_webserver_start(http_handlers);
	mesh_start(mesh_handlers, ANY_ADDR, 6636);

	uint32_t end_time = system_get_time();
	os_printf("time: system up by: %umks\n", (end_time - start_time));
}

