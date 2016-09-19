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

#define WIFI_EVENT_HANDLER_STACK_SIZE 128
#define WIFI_EVENT_HANDLER_PRIO DEFAULT_TASK_PRIO 
#define WIFI_EVENT_WAIT_TIMER 10000
#define WIFI_EVENT_POOL_SIZE 10

#define WIFI_STATION_CHECK_TIMER 5000

static const char* reasons[] = 
{
	"good",
    "REASON_UNSPECIFIED",
    "REASON_AUTH_EXPIRE",
    "REASON_AUTH_LEAVE",
    "REASON_ASSOC_EXPIRE",
    "REASON_ASSOC_TOOMANY",
    "REASON_NOT_AUTHED",
    "REASON_NOT_ASSOCED",
    "REASON_ASSOC_LEAVE",
    "REASON_ASSOC_NOT_AUTHED",
    "REASON_DISASSOC_PWRCAP_BAD",
    "REASON_DISASSOC_SUPCHAN_BAD",
	"SKIP",
    "REASON_IE_INVALID",
    "REASON_MIC_FAILURE",
    "REASON_4WAY_HANDSHAKE_TIMEOUT",
    "REASON_GROUP_KEY_UPDATE_TIMEOUT",
    "REASON_IE_IN_4WAY_DIFFERS",
    "REASON_GROUP_CIPHER_INVALID",
    "REASON_PAIRWISE_CIPHER_INVALID",
    "REASON_AKMP_INVALID",
    "REASON_UNSUPP_RSN_IE_VERSION",
    "REASON_INVALID_RSN_IE_CAP",
    "REASON_802_1X_AUTH_FAILED",
    "REASON_CIPHER_SUITE_REJECTED",

    "REASON_BEACON_TIMEOUT",
    "REASON_NO_AP_FOUND",
    "REASON_AUTH_FAIL",
    "REASON_ASSOC_FAIL",
    "REASON_HANDSHAKE_TIMEOUT",
};

static xQueueHandle wifi_event_handler_stop = NULL;
static xQueueHandle wifi_event_queue = NULL;

static WIFI_MODE current_mode = NULL_MODE;
static os_timer_t check_station_timer;

char target_ssid[MAX_SSID_LENGTH] = { 0 };
static uint8_t last_error = 0;

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

static void wifi_check_station_state()
{
	if(wifi_get_opmode() == STATION_MODE)
	{
		if(last_error != 0)
		{
			os_printf("wifi: failed station connect, error detected\n");

			if(!wifi_set_opmode_current(STATIONAP_MODE))
			{
				os_printf("wifif: failed restore ap mode\n");
			}
		}
	}
	os_timer_disarm(&check_station_timer);
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

						last_error = 0;
						memset(target_ssid, 0, MAX_SSID_LENGTH);
						memcpy(target_ssid, event->event_info.connected.ssid, strlen(event->event_info.connected.ssid));
					break;

					case EVENT_STAMODE_DISCONNECTED:
						os_printf("wifi: event EVENT_STAMODE_DISCONNECTED\n");
						os_printf("wifi: disconnected from: %s, reason: %d\n", event->event_info.disconnected.ssid, event->event_info.disconnected.reason);
						if(strnlen(target_ssid, MAX_SSID_LENGTH) > 0 && strncmp(target_ssid, event->event_info.disconnected.ssid, MAX_SSID_LENGTH) == 0)
						{
							last_error = event->event_info.disconnected.reason;
						}
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


bool start_station_wifi(struct wifi_info* info, bool connect)
{
	wifi_event_task_start();
	return set_station_info(info, connect);
}

bool start_ap_wifi(struct device_info* info)
{
	bool result = true;
	wifi_event_task_start();
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

bool set_station_info(struct wifi_info* info, bool connect)
{
	bool result = true;
	if(info != NULL)
	{
		struct station_config config;
		memset(&config, 0, sizeof(struct station_config));

		config.bssid_set = 0;
		memcpy(config.ssid, info->name, strnlen(info->name, WIFI_NAME_SIZE));
		memcpy(config.password, info->pass, strnlen(info->pass, WIFI_PASS_SIZE));

		os_printf("wifi: name: `%s`, pass: `%s` \n", config.ssid, config.password);

		if(!wifi_set_opmode_current(STATION_MODE))
		{
			os_printf("wifi: failed set opmode\n");
			result = false;
		}

		if(result && !wifi_station_set_config_current(&config))
		{
			os_printf("wifi: failed set station config\n");
			result = false;
		}

		if(result && connect)
		{
			wifi_station_disconnect();
			if(!wifi_station_connect())
			{
				os_printf("wifi: failed station connect\n");
				result = false;
			}
			else if(info->ip == 0)
			{
				if(!wifi_station_dhcpc_start())
				{
					os_printf("wifi: failed start dhcpc\n");
					result = false;
				}
			}
			/*else if(info->ip != 0)*/
			/*{*/
				/*if(wifi_station_dhcpc_status() != DHCP_STOPPED)*/
				/*{*/
					/*if(!wifi_station_dhcpc_stop())*/
					/*{*/
						/*os_printf("wifi: failed stop dhcpc\n");*/
						/*result = false;*/
					/*}*/
				/*}*/

				/*if(result)*/
				/*{*/
					/*struct ip_info ip_info;*/
					/*ip_info.ip.addr = info->ip;*/
					/*ip_info.gw.addr = info->gw;*/
					/*ip_info.netmask.addr = info->mask;*/

					/*if(!wifi_set_ip_info(STATION_IF, &ip_info))*/
					/*{*/
						/*os_printf("wifi: failed set ip info\n");*/
						/*result = false;*/
					/*}*/
				/*}*/
			/*}*/
		}

		if(result)
		{ // завод таймера для проверки соеденения
			last_error = 0;
			memset(target_ssid, 0, MAX_SSID_LENGTH);
			memcpy(target_ssid, config.ssid, strlen(config.ssid));

			os_timer_setfn(&check_station_timer, wifi_check_station_state, NULL);
			os_timer_arm(&check_station_timer, WIFI_STATION_CHECK_TIMER, false);
		}
	}
	else
	{
		os_printf("wifi: info is NULL\n");
		result = false;
	}
	return result;
}

const char* wifi_get_last_error()
{
	uint8_t index = last_error;
	if(index >= 200)
	{
		index -= 175;
	}
	return index != 0 ? reasons[index] : NULL;
}

bool get_wifi_ip_info(struct ip_info* ip_info)
{
	WIFI_INTERFACE i = wifi_get_opmode() == STATION_MODE ?  STATION_IF : SOFTAP_IF;
	return wifi_get_ip_info(i, ip_info);
}
