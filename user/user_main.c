#include "user_config.h"

#include "esp_common.h"
#include "wifi_station.h"
#include "light_http.h"

#include "driver/uart.h"

#include "../esp-gdbstub/gdbstub.h"

const char *FlashSizeMap[] =
{
		"512 KB (256 KB + 256 KB)",	// 0x00
		"256 KB",			// 0x01
		"1024 KB (512 KB + 512 KB)", 	// 0x02
		"2048 KB (512 KB + 512 KB)"	// 0x03
		"4096 KB (512 KB + 512 KB)"	// 0x04
		"2048 KB (1024 KB + 1024 KB)"	// 0x05
		"4096 KB (1024 KB + 1024 KB)"	// 0x06
};

LOCAL os_timer_t blink_timer;

/*LOCAL void ICACHE_FLASH_ATTR blinker(void *p_args)*/
LOCAL void blinker(void *p_args)
{
	os_printf("==== System info: ====\r\n");
	os_printf("SDK version:%s rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check());
	/*os_printf("Time = %ld\r\n", system_get_time());*/
	os_printf("Chip id = 0x%x\r\n", system_get_chip_id());
	os_printf("CPU freq = %d MHz\r\n", system_get_cpu_freq());
	os_printf("Flash size map = %s\r\n", FlashSizeMap[system_get_flash_size_map()]);
	os_printf("Free heap size = %d\r\n", system_get_free_heap_size());
	os_printf("==== End System info ====\r\n");
}


//!!!WORKED print to uart
void user_init(void)
{
	uart_init_new();
	UART_SetBaudrate(UART0, BIT_RATE_115200);

	gdbstub_init();

	os_timer_setfn(&blink_timer, blinker, NULL);
	os_timer_arm(&blink_timer, 2000, true);

	start_wifi();
	webserver_start();
}

