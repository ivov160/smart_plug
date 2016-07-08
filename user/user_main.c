#include "user_config.h"

#include "esp_common.h"
#include "wifi_station.h"
#include "light_http.h"

#include "driver/uart.h"

/*#include "esp_gdb.h"*/
/*#include "esp_exc.h"*/

#include "../esp-gdbstub/gdbstub.h"
/*#include "gdb_exception.h"*/

#define STATIC_STRLEN(x) (sizeof(x) - 1)

LOCAL os_timer_t blink_timer;
LOCAL int blink_state = 1;

LOCAL void ICACHE_FLASH_ATTR blinker(void *p_args)
{
	/*printf("SDK version:%s\n", system_get_sdk_version());*/
	/*printf("SDK version:%s\n", system_get_sdk_version());*/
	/*printf("test: %d", 1);*/
	/*os_printf("test: %d\n", 1);*/
	/*os_printf("SDK version:%s\n", system_get_sdk_version());*/
	/*os_printf(">>> heapsize %d, sizeof `GET `: %d\n", system_get_free_heap_size(), STATIC_STRLEN("GET "));*/
}


//!!!WORKED print to uart
void user_init(void)
{
	uart_init_new();
	UART_SetBaudrate(UART0, BIT_RATE_115200);

	/*esp_exception_handler_init();*/
	gdbstub_init();
	/*gdb_init();*/

	os_timer_setfn(&blink_timer, blinker, NULL);
	os_timer_arm(&blink_timer, 1000, true);

	start_wifi();
	webserver_start();
}

