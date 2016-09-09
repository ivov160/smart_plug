#include "wifi_station.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_common.h"
#include "espressif/esp_wifi.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define MAX_AP_CONNECTION 10
#define PASSWORD_LENGTH 8
#define MAX_SSID_LENGTH 32

#define WIFI_EVENT_HANDLER_STACK_SIZE 512
#define WIFI_EVENT_HANDLER_PRIO DEFAULT_TASK_PRIO 
#define WIFI_EVENT_WAIT_TIMER 10000
#define WIFI_EVENT_POOL_SIZE 10

static xQueueHandle wifi_event_handler_stop = NULL;
static xQueueHandle wifi_event_queue = NULL;

static char int_to_hex[] = "0123456789ABCDEF";

static void wifi_event_handler(System_Event_t *event)
{
	if(wifi_event_queue != NULL)
	{
		portBASE_TYPE call_yield = pdFALSE;
		uintptr_t tmp = (uintptr_t) event;
		if(xQueueSendFromISR(wifi_event_queue, &tmp, &call_yield) != pdPASS)
		{
			os_printf("wifi: failed push event to wifi_event_queue\n");
		}
		/*else*/
		/*{*/
			/*if(call_yield)*/
			/*{	*/
				/*portYIELD_FROM_ISR();*/
			/*}*/
		/*}*/
	}
	else
	{
		os_printf("wifif: wifi_event_queue is null\n");
	}
}

///@todo заменить на что либо, или реализовать в 1 месте
static bool check_stop_condition_wifi(xQueueHandle *queue)
{
	bool result = true;
	if(queue != NULL)
	{
		portBASE_TYPE xStatus = xQueueReceive(*queue, &result, 0);
		if (pdPASS == xStatus && result == TRUE)
		{
			result = true;
		}
		else
		{
			result = false;
		}
	}
	else
	{
		os_printf("wibi: check_stop_condition_wifi invalid pointer\n");
	}
	return result;
}

static void wifi_event_handler_task(void *pvParameters)
{
	while(true)
	{
		// checking stop flag
		if(check_stop_condition_wifi(&wifi_event_handler_stop))
		{
			os_printf("webserver: webserver_task rcv exit signal!\n");
			break;
		}

		uintptr_t tmp = NULL;
		if(!xQueueReceive(wifi_event_queue, &tmp, WIFI_EVENT_WAIT_TIMER / portTICK_RATE_MS))
		{
			taskYIELD();
		}
		else
		{
			System_Event_t* event = (System_Event_t*) tmp;
			if(event != NULL)
			{
				switch (event->event_id) 
				{
					case EVENT_STAMODE_SCAN_DONE:
						os_printf("wifi: event EVENT_STAMODE_SCAN_DONE\n");
					break;

					case EVENT_STAMODE_CONNECTED:
						os_printf("wifi: event EVENT_STAMODE_CONNECTED\n");
					break;

					case EVENT_STAMODE_DISCONNECTED:
						os_printf("wifi: event EVENT_STAMODE_DISCONNECTED\n");
					break;

					case EVENT_STAMODE_AUTHMODE_CHANGE:
						os_printf("wifi: event EVENT_STAMODE_AUTHMODE_CHANGE\n");
					break;

					case EVENT_STAMODE_GOT_IP:
						os_printf("wifi: event EVENT_STAMODE_GOT_IP\n");
					break;

					case EVENT_STAMODE_DHCP_TIMEOUT:
						os_printf("wifi: event EVENT_STAMODE_DHCP_TIMEOUT\n");
					break;

					case EVENT_SOFTAPMODE_STACONNECTED:
						os_printf("wifi: event EVENT_SOFTAPMODE_STACONNECTED\n");
					break;

					case EVENT_SOFTAPMODE_STADISCONNECTED:
						os_printf("wifi: event EVENT_SOFTAPMODE_STADISCONNECTED\n");
					break;

					case EVENT_SOFTAPMODE_PROBEREQRECVED:
						os_printf("wifi: event EVENT_SOFTAPMODE_PROBEREQRECVED\n");
					break;
				};
			}
			else
			{
				os_printf("wifi: getted null ptr event\n");
			}
		}
	}
    vQueueDelete(wifi_event_handler_stop);
    wifi_event_handler_stop = NULL;
    vTaskDelete(NULL);
}

static void wifi_event_task_stop()
{
	if(wifi_event_handler_stop != NULL)
	{
		bool ValueToSend = true;
		portBASE_TYPE xStatus = xQueueSend(wifi_event_handler_stop, &ValueToSend, 0);
		if (xStatus != pdPASS)
		{
			os_printf("wifi: Could not send stop to wifi_event_handler_task\n");
		} 
		else
		{
			taskYIELD();
		}
	}

	if(wifi_event_queue != NULL)
	{
		vQueueDelete(wifi_event_queue);
		wifi_event_queue = NULL;
	}
}

static void wifi_event_task_start()
{
	if(wifi_set_event_handler_cb(wifi_event_handler))
	{
		if(wifi_event_handler_stop == NULL)
		{
			wifi_event_handler_stop = xQueueCreate(1, sizeof(bool));
		}

		if(wifi_event_queue == NULL)
		{
			wifi_event_queue = xQueueCreate(WIFI_EVENT_POOL_SIZE, sizeof(uintptr_t));
			xTaskCreate(wifi_event_handler_task, "wifi_event_handler", WIFI_EVENT_HANDLER_STACK_SIZE, NULL, WIFI_EVENT_HANDLER_PRIO, NULL);
		}
	}
	else
	{
		os_printf("wifi: regiser wifi_event_handler failed\n");
	}
}

static void generate_password(char* password, uint32_t size)
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


bool start_station_wifi(struct wifi_info* info)
{
	wifi_event_task_start();
	return set_station_info(info);
}

bool start_ap_wifi(struct device_info* info)
{
	bool result = true;
	/*wifi_event_task_start();*/
	if(info != NULL)
	{
		struct softap_config ap_config;
		if(!wifi_set_opmode_current(STATIONAP_MODE))
		{
			os_printf("wifi: failed initialize STATIONAP_MODE\n");
			result = false;
		}
		else
		{
			const char* device_type_str = device_info_get_type(info);
			char ssid[MAX_SSID_LENGTH] = { 0 };
			char password[PASSWORD_LENGTH + 1] = { 0 };

			generate_password(password, PASSWORD_LENGTH);

			int render_size = sprintf(ssid, "LOOKin_%s_%s", device_type_str, password);
			if(render_size < 0)
			{
				os_printf("wifi: failed generate ssid\n");
				result = false;
			}

			memcpy(ap_config.ssid, ssid, render_size);
			ap_config.ssid_len = render_size;

			memcpy(ap_config.password, password, PASSWORD_LENGTH + 1);

			ap_config.channel = 1;
			ap_config.authmode = AUTH_WPA2_PSK;
			ap_config.ssid_hidden = 0;

			ap_config.max_connection = MAX_AP_CONNECTION;

			os_printf("wifi: ssid: %s, render_size: %d, password: %s\n", ap_config.ssid, render_size, ap_config.password);
		}

		if(result && !wifi_softap_set_config_current(&ap_config))
		{
			os_printf("wifi: failed set softap_config\n");
			result = false;
		}

		if(result && !wifi_softap_dhcps_start())
		{
			os_printf("wifi: failed start dhcp server\n");
			result = false;
		}
	}
	else
	{
		os_printf("wifi: device_info is null\n");
		result = false;
	}
	return result;
}

void start_wifi(struct device_info* info)
{
	/*bool success = false;*/
	/*for(uint32_t station_tries = 0; station_tries < 5; ++station_tries)*/
	/*{*/
		/*if(start_station_wifi())*/
		/*{*/
			/*success = true;*/
			/*break;*/
		/*}*/
	/*}*/
	/*if(!success)*/
	/*{*/
		/*start_ap_wifi(info);*/
	/*}*/
}

void stop_wifi(bool cleanup)
{
	if(cleanup)
	{
		wifi_event_task_stop();
	}

	if(wifi_get_opmode() == STATION_MODE)
	{
		if(wifi_station_dhcpc_status() != DHCP_STOPPED && !wifi_station_dhcpc_stop())
		{
			os_printf("wifi: failed stop dhcpc\n");
		}

		if(!wifi_station_disconnect())
		{
			os_printf("wifi: failed disconnect from station\n");
		}

	}
	else
	{
		if(DHCP_STARTED == wifi_softap_dhcps_status() && !wifi_softap_dhcps_stop())
		{
			os_printf("wifi: failed stop dhcps\n");
		}
	}

	if(!wifi_set_opmode_current(NULL_MODE))
	{
		os_printf("wifi: failed disable wifi\n");
	}
}

bool set_station_info(struct wifi_info* info)
{
	bool result = false;
	if(info != NULL)
	{
		os_printf("wifi: connect to station: %s\n", info->name);

		struct station_config config;
		config.bssid_set = 0;
		memcpy(config.ssid, info->name, strnlen(info->name, WIFI_NAME_SIZE));
		memcpy(config.password, info->pass, strnlen(info->pass, WIFI_PASS_SIZE));

		/*if(wifi_station_set_config(&config))*/
		if(wifi_station_set_config_current(&config))
		{
			bool scope_result = true;

			if(wifi_get_opmode() != STATION_MODE && !wifi_set_opmode_current(STATION_MODE))
			{
				os_printf("wifi: failed initialize STATION_MODE\n");
				scope_result = false;
			}

			if(scope_result && info->ip != 0)
			{
				if(wifi_station_dhcpc_status() != DHCP_STOPPED)
				{
					if(!wifi_station_dhcpc_stop())
					{
						scope_result = false;
						os_printf("wifi: failed stop dhcpc\n");
					}
				}

				if(scope_result)
				{
					struct ip_info ip_info;
					ip_info.ip.addr = info->ip;
					ip_info.gw.addr = info->gw;
					ip_info.netmask.addr = info->mask;

					if(!wifi_set_ip_info(STATION_IF, &ip_info))
					{
						scope_result = false;
						os_printf("wifi: failed set ip info\n");
					}
				}
			}
			else if(wifi_station_dhcpc_status() != DHCP_STARTED)
			{
				if(!wifi_station_dhcpc_start())
				{
					scope_result = false;
					os_printf("wifi: failed start dhcpc\n");
				}
			}

			if(scope_result && !wifi_station_connect())
			{
				os_printf("wifi: failed connection to station\n");
				scope_result = false;
			}
			result = true & scope_result;
		}
		else
		{
			os_printf("wifi: failed connect to wifi station\n");
		}
	}
	else
	{
		os_printf("wifi: wifi_info is NULL\n");
	}
	return result;
}
