#include "user_http_handlers.h"
#include "user_power.h"
#include "user_wifi.h"

#include "../data/data.h"

#include "cJSON.h"
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/ip_addr.h"

#include <stdlib.h>
#include <ctype.h>

/**
 * @defgroup user User 
 * @defgroup user_light_http User light_http
 *
 * @addtogroup user
 * @{
 * @addtogroup user_light_http 
 * @{
 */

#define STATIC_STRLEN(x) (sizeof(x) - 1)

#define INITIATOR_TIMER 1000
#define INITIATOR_POOL_SIZE 5

static os_timer_t wifi_connect_timer;

static xQueueHandle scan_callback_initiators  = NULL;

char* make_lower(const char* str)
{
	char* low_case = NULL;
	if(str != NULL)
	{
		low_case = (char*) zalloc(strlen(str));

		char* target_ptr = low_case;
		const char* ptr = str;

		while(ptr && *ptr != '\0')
		{
			*target_ptr = tolower(*ptr);
			++ptr;
			++target_ptr;
		}
	}
	return low_case;
}

static void wifi_reconnect_callback(struct query* query, void* data)
{
	struct data_wifi_info* info = (struct data_wifi_info*) data;
	if(!wifi_start_station(info, true))
	{
		os_printf("http: failed reconect to new wifi station\n");
	}
	free(info);
}

/**
 * @bug Может произойти segmentation fault 
 * Для получения query используется очередь, в которую добавляются указательи на query, ассоциированные с обрабатываемым запросом
 * Но если connection закроется раньше чем данный callback сформирует ответ и пометит запрос как обработанный
 * указатель будет не валидным.
 * Решением является добавление в connection счетчика сылок (shared_ptr, но для C)
 * @todo Реализовать счетчика сылок для query (shared_ptr<struct query>, но для C)
 */
static void http_scan_callback(void *args, STATUS status)
{
	uintptr_t item = 0;
	if(xQueueReceive(scan_callback_initiators, &item, INITIATOR_TIMER / portTICK_RATE_MS))
	{
		struct query* query = (struct query*) item;
		cJSON *json_root = cJSON_CreateObject();

		if(status == OK)
		{
			if(args != NULL)
			{
				cJSON_AddBoolToObject(json_root, "success", true);
				cJSON *json_data = cJSON_CreateArray();

				struct bss_info* bss_list = (struct bss_info*) args;
				struct bss_info *bss = bss_list;

				while(bss != NULL)
				{
					os_printf("wifi: scaned ssid: `%s`\n", bss->ssid);
					cJSON *json_value = cJSON_CreateString(bss->ssid);
					cJSON_AddItemToArray(json_data, json_value);
					bss = STAILQ_NEXT(bss, next);
				}
				/*free(args);*/
				cJSON_AddItemToObject(json_root, "data", json_data);
			}
			else
			{
				os_printf("wifi: scan results arg is NULL\n");
				cJSON_AddBoolToObject(json_root, "success", false);
			}
		}
		else
		{
			cJSON_AddBoolToObject(json_root, "success", false);
		}

		char* data = cJSON_Print(json_root);

		query_response_status(200, query);
		query_response_header("Content-Type", "application/json", query);
		query_response_body(data, strlen(data), query);

		cJSON_Delete(json_root);
		free(data);

		query_done(query);
	}
	else
	{
		os_printf("http: failed fetch callback initiator\n");
	}
}

int http_system_info_handler(struct query *query)
{
	query_response_status(200, query);

	query_response_header("Content-Type", "application/json", query);

	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddBoolToObject(json_root, "success", true);

	cJSON *json_data = cJSON_CreateObject();
	cJSON_AddItemToObject(json_root, "data", json_data);

	cJSON_AddStringToObject(json_data, "sdk_version", system_get_sdk_version());
	cJSON_AddNumberToObject(json_data, "chip_id", system_get_chip_id());
	cJSON_AddNumberToObject(json_data, "cpu", system_get_cpu_freq());
	cJSON_AddNumberToObject(json_data, "heap_size", system_get_free_heap_size());

	char* data = cJSON_Print(json_root);
	query_response_body(data, strlen(data), query);

	free(data);
	cJSON_Delete(json_root);

	return 1;
}

int http_get_device_info_handler(struct query *query)
{
	struct ip_info ip_info;
	struct data_device_info info;
	struct data_custom_name name_info;

	memset(&name_info, 0, sizeof(struct data_custom_name));
	memset(&info, 0, sizeof(struct data_device_info));

	cJSON *json_root = cJSON_CreateObject();

	if(data_read_custom_name(&name_info) && data_read_current_device(&info) && wifi_get_ip(&ip_info))
	{
		cJSON *json_data = cJSON_CreateObject();
		cJSON_AddItemToObject(json_root, "data", json_data);

		char ip_print_buffer[4 * 4 + 1] = { 0 };
		sprintf(ip_print_buffer, IPSTR, IP2STR(&ip_info.ip));

		cJSON_AddNumberToObject(json_data, "powered", data_device_info_get_powered(&info));
		cJSON_AddNumberToObject(json_data, "type", data_device_info_get_type_int(&info));

		uint32_t length = strnlen(name_info.data, DATA_CUSTOM_NAME_SIZE);
		if(length != 0 && length != DATA_CUSTOM_NAME_SIZE)
		{
			cJSON_AddStringToObject(json_data, "name", name_info.data);
		}
		else
		{
			cJSON_AddNullToObject(json_data, "name");
		}

		cJSON_AddStringToObject(json_data, "ip", ip_print_buffer);
		cJSON_AddBoolToObject(json_root, "success", true);
	}
	else
	{
		cJSON_AddBoolToObject(json_root, "success", false);
	}

	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	free(data);
	cJSON_Delete(json_root);

	return 1;
}

int http_scan_wifi_info_list_handler(struct query *query)
{
	if(scan_callback_initiators == NULL)
	{
		scan_callback_initiators = xQueueCreate(INITIATOR_POOL_SIZE, sizeof(uintptr_t));
	}

	int result = 0;
	uintptr_t item = (uintptr_t)query;
	if(!xQueueSend(scan_callback_initiators, &item, INITIATOR_TIMER / portTICK_RATE_MS))
	{
		result = 1;

		cJSON *json_root = cJSON_CreateObject();
		cJSON_AddBoolToObject(json_root, "success", false);
		char* data = cJSON_Print(json_root);

		query_response_status(200, query);
		query_response_header("Content-Type", "application/json", query);
		query_response_body(data, strlen(data), query);

		cJSON_Delete(json_root);
		free(data);
	}
	else
	{
		if(!wifi_station_scan(NULL, http_scan_callback))
		{
			os_printf("http: failed start wifi scan\n");
		}
		result = 0;
	}
	return result;
}


int http_set_device_name_handler(struct query *query)
{
	int result = 0;
	const char* device_name = query_get_param("name", query, REQUEST_POST);
	if(device_name != NULL)
	{
		os_printf("http: new device name: `%s`\n", device_name);

		int32_t device_name_size = strnlen(device_name, DATA_CUSTOM_NAME_SIZE - 1);
		if(device_name_size < DATA_CUSTOM_NAME_SIZE - 1)
		{
			struct data_custom_name name;
			memset(&name, 0, sizeof(struct data_custom_name));
			memcpy(name.data, device_name, device_name_size);

			if(data_write_custom_name(&name))
			{
				result = 1;
			}
		}
		else
		{
			os_printf("http: too long new device name\n");
		}
	}
	else
	{
		os_printf("http: empty new device name\n");
	}

	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddBoolToObject(json_root, "success", (result ? true : false));
	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

int http_set_main_wifi_handler(struct query *query)
{
	int result = 0;

	struct data_wifi_info* main_wifi = (struct data_wifi_info*) zalloc(sizeof(struct data_wifi_info));

	const char* param = query_get_param("name", query, REQUEST_POST);
	if(param != NULL && strnlen(param, DATA_WIFI_NAME_SIZE) < DATA_WIFI_NAME_SIZE)
	{
		memcpy(main_wifi->name, param, strnlen(param, DATA_WIFI_NAME_SIZE));
		param = query_get_param("pass", query, REQUEST_POST);
		if(param != NULL && strnlen(param, DATA_WIFI_PASS_SIZE) < DATA_WIFI_PASS_SIZE)
		{
			memcpy(main_wifi->pass, param, strnlen(param, DATA_WIFI_PASS_SIZE));
			param = query_get_param("ip", query, REQUEST_POST);
			if(param != NULL && strncmp(param, "dhcp", sizeof("dhcp") - 1) != 0)
			{	// not dhcp
				main_wifi->ip = ipaddr_addr(param);
				if(main_wifi->ip != 0)
				{
					param = query_get_param("mask", query, REQUEST_POST);
					if(param != NULL)
					{
						main_wifi->mask = ipaddr_addr(param);
						if(main_wifi->mask != 0)
						{
							param = query_get_param("gw", query, REQUEST_POST);
							if(param != NULL)
							{
								main_wifi->gw = ipaddr_addr(param);
								if(main_wifi->gw != 0)
								{
									param = query_get_param("dns", query, REQUEST_POST);
									if(param != NULL)
									{
										main_wifi->dns = ipaddr_addr(param);
										if(main_wifi->dns != 0)
										{	// finish point
											result = 1;
										}
										else
										{
											os_printf("http: wifi dns invalid string\n");
										}
									}
									else
									{
										os_printf("http: wifi dns is not setted\n");
									}
								}
								else
								{
									os_printf("http: wifi gw invalid string\n");
								}
							}
							else
							{
								os_printf("http: wifi gw is not setted\n");
							}
						}
						else
						{
							os_printf("http: wifi mask invalid string\n");
						}
					}
					else
					{
						os_printf("http: wifi mask is not setted\n");
					}
				}
				else
				{
					os_printf("http: wifi ip invalid string\n");
				}
			}
			else if(param == NULL)
			{
				os_printf("http: wifi ip is not setted\n");
			}
			else
			{	// ip dhcp
				result = true;
			}
		}
		else
		{
			os_printf("http: wifi pass is not setted\n");
		}
	}
	else
	{
		os_printf("http: wifi name is not setted\n");
	}

	if(result)
	{
		if(!data_write_main_wifi(main_wifi))
		{
			result = false;
			os_printf("http: failed set station info\n");
		}
	}

	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddBoolToObject(json_root, "success", (result ? true : false));
	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	if(result)
	{
		query_register_after_response(query, wifi_reconnect_callback, (void*) main_wifi);
	}

	return 1;
}

int http_get_wifi_error_handler(struct query *query)
{
	cJSON *json_root = cJSON_CreateObject();

	const char* error = wifi_get_last_error();
	if(error != NULL)
	{
		cJSON *json_data = cJSON_CreateObject();
		cJSON_AddStringToObject(json_data, "error", error);

		cJSON_AddItemToObject(json_root, "data", json_data);
		cJSON_AddBoolToObject(json_root, "success", true);
	}
	else
	{
		cJSON_AddBoolToObject(json_root, "success", false);
	}

	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

int http_on_handler(struct query *query)
{
	const char* type = query_get_param("type", query, REQUEST_GET);
	char* lower_type = make_lower(type);

	if(lower_type == NULL || strncmp(lower_type, "string", STATIC_STRLEN("string")) == 0)
	{
		power_up();
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		query_response_body("success", STATIC_STRLEN("success"), query);
	}
	else if(lower_type != NULL && strncmp(lower_type, "bool", STATIC_STRLEN("bool")) == 0)
	{
		power_up();
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		query_response_body("255", STATIC_STRLEN("255"), query);
	}
	else if(lower_type != NULL && strncmp(lower_type, "json", STATIC_STRLEN("json")) == 0)
	{
		power_up();

		cJSON *json_root = cJSON_CreateObject();
		cJSON_AddBoolToObject(json_root, "success", true);
		char* data = cJSON_Print(json_root);

		query_response_status(200, query);
		query_response_header("Content-Type", "application/json", query);
		query_response_body(data, strlen(data), query);

		cJSON_Delete(json_root);
		free(data);
	}
	else
	{
		query_response_status(400, query);
	}

	if(lower_type != NULL)
	{
		free(lower_type);
	}

	return 1;
}

int http_off_handler(struct query *query)
{
	const char* type = query_get_param("type", query, REQUEST_GET);
	char* lower_type = make_lower(type);

	if(lower_type == NULL || strncmp(lower_type, "string", STATIC_STRLEN("string")) == 0)
	{
		power_down();
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		query_response_body("success", STATIC_STRLEN("success"), query);
	}
	else if(lower_type != NULL && strncmp(lower_type, "bool", STATIC_STRLEN("bool")) == 0)
	{
		power_down();
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		query_response_body("255", STATIC_STRLEN("255"), query);
	}
	else if(lower_type != NULL && strncmp(lower_type, "json", STATIC_STRLEN("json")) == 0)
	{
		power_down();

		cJSON *json_root = cJSON_CreateObject();
		cJSON_AddBoolToObject(json_root, "success", true);
		char* data = cJSON_Print(json_root);

		query_response_status(200, query);
		query_response_header("Content-Type", "application/json", query);
		query_response_body(data, strlen(data), query);

		cJSON_Delete(json_root);
		free(data);
	}
	else
	{
		query_response_status(400, query);
	}

	if(lower_type != NULL)
	{
		free(lower_type);
	}

	return 1;
}

int http_status_handler(struct query *query)
{
	bool status = power_status() == 1;

	const char* type = query_get_param("type", query, REQUEST_GET);
	char* lower_type = make_lower(type);

	if(lower_type == NULL || strncmp(lower_type, "string", STATIC_STRLEN("string")) == 0)
	{
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		query_response_body(status ? "on" : "off", STATIC_STRLEN(status ? "on" : "off"), query);
	}
	else if(lower_type != NULL && strncmp(lower_type, "bool", STATIC_STRLEN("bool")) == 0)
	{
		query_response_status(200, query);
		query_response_header("Content-Type", "text/html", query);
		if(status)
		{
			query_response_body("255", STATIC_STRLEN("255"), query);
		}
		else
		{
			query_response_body("0", 1, query);
		}
	}
	else if(lower_type != NULL && strncmp(lower_type, "json", STATIC_STRLEN("json")) == 0)
	{
		cJSON *json_root = cJSON_CreateObject();
		cJSON_AddBoolToObject(json_root, "success", true);

		cJSON *json_data = cJSON_CreateObject();
		cJSON_AddItemToObject(json_root, "data", json_data);

		cJSON_AddBoolToObject(json_data, "power", status);

		char* data = cJSON_Print(json_root);

		query_response_status(200, query);
		query_response_header("Content-Type", "application/json", query);
		query_response_body(data, strlen(data), query);

		cJSON_Delete(json_root);
		free(data);
	}
	else
	{
		query_response_status(400, query);
	}

	if(lower_type != NULL)
	{
		free(lower_type);
	}

	return 1;
}

int http_start_test_mode(struct query *query)
{
	cJSON *json_root = cJSON_CreateObject();

	const char* timer_str = query_get_param("timeout", query, REQUEST_POST);
	uint32_t timeout = 0;
		
	if(timer_str != NULL && (timeout = atoi(timer_str)) != 0)
	{
		power_start_test_mode(timeout);
		cJSON_AddBoolToObject(json_root, "success", true);
	}
	else
	{
		cJSON_AddBoolToObject(json_root, "success", false);
	}

	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

int http_stop_test_mode(struct query *query)
{
	cJSON *json_root = cJSON_CreateObject();

	cJSON_AddBoolToObject(json_root, "success", true);
	power_stop_test_mode();
		
	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

/**
 * @}
 * @}
 */

