#include "user_http_handlers.h"
#include "user_power.h"
#include "wifi_station.h"

#include "../flash/flash.h"

#include "cJSON.h"
#include "esp_common.h"

#include "lwip/ip_addr.h"

#define STATIC_STRLEN(x) (sizeof(x) - 1)

static os_timer_t wifi_connect_timer;
static struct query* ptr = NULL;

static void wifi_connect_callback(void *p_args)
{
	struct wifi_info* info = (struct wifi_info*) p_args;
	if(!start_station_wifi(info, true))
	{
		os_printf("http: failed reconect to new wifi station\n");
	}
	free(info);
}

static void wifi_reconnect_callback(struct query* query, void* data)
{
	struct wifi_info* info = (struct wifi_info*) data;
	if(!start_station_wifi(info, true))
	{
		os_printf("http: failed reconect to new wifi station\n");
	}
	free(info);
}

static void http_scan_callback(void *args, STATUS status)
{
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

	query_response_status(200, ptr);
	query_response_header("Content-Type", "application/json", ptr);
	query_response_body(data, strlen(data), ptr);

	cJSON_Delete(json_root);
	free(data);

	query_done(ptr);
	ptr = NULL;
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
	struct device_info info;
	struct custom_name name_info;

	memset(&name_info, 0, sizeof(struct custom_name));
	memset(&info, 0, sizeof(struct device_info));

	cJSON *json_root = cJSON_CreateObject();

	if(read_custom_name(&name_info) && read_current_device(&info) && get_wifi_ip_info(&ip_info))
	{
		cJSON *json_data = cJSON_CreateObject();
		cJSON_AddItemToObject(json_root, "data", json_data);

		char ip_print_buffer[4 * 4 + 1] = { 0 };
		sprintf(ip_print_buffer, IPSTR, IP2STR(&ip_info.ip));

		cJSON_AddNumberToObject(json_data, "powered", device_info_get_powered(&info));
		cJSON_AddNumberToObject(json_data, "type", device_info_get_type_int(&info));

		uint32_t length = strnlen(name_info.data, CUSTOM_NAME_SIZE);
		if(length != 0 && length != CUSTOM_NAME_SIZE)
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
	int result = 0;
	if(ptr != NULL)
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
		ptr = query;
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

		int32_t device_name_size = strnlen(device_name, CUSTOM_NAME_SIZE - 1);
		if(device_name_size < CUSTOM_NAME_SIZE - 1)
		{
			struct custom_name name;
			memset(&name, 0, sizeof(struct custom_name));
			memcpy(name.data, device_name, device_name_size);

			if(write_custom_name(&name))
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

	struct wifi_info* main_wifi = (struct wifi_info*) zalloc(sizeof(struct wifi_info));

	const char* param = query_get_param("name", query, REQUEST_POST);
	if(param != NULL && strnlen(param, WIFI_NAME_SIZE) < WIFI_NAME_SIZE)
	{
		memcpy(main_wifi->name, param, strnlen(param, WIFI_NAME_SIZE));
		param = query_get_param("pass", query, REQUEST_POST);
		if(param != NULL && strnlen(param, WIFI_PASS_SIZE) < WIFI_PASS_SIZE)
		{
			memcpy(main_wifi->pass, param, strnlen(param, WIFI_PASS_SIZE));
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
		if(!write_main_wifi(main_wifi))
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
		/*os_timer_setfn(&wifi_connect_timer, wifi_connect_callback, main_wifi);*/
		/*os_timer_arm(&wifi_connect_timer, 500, false);*/
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
	power_up();

	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddBoolToObject(json_root, "success", true);
	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

int http_off_handler(struct query *query)
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

	return 1;
}

int http_status_handler(struct query *query)
{
	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddBoolToObject(json_root, "success", true);

	cJSON *json_data = cJSON_CreateObject();
	cJSON_AddItemToObject(json_root, "data", json_data);

	cJSON_AddBoolToObject(json_data, "power", power_status() == 1);

	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	cJSON_Delete(json_root);
	free(data);

	return 1;
}

