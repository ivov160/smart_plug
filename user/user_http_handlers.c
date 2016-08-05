#include "user_http_handlers.h"
#include "../flash/flash.h"

#include "cJSON.h"
#include "esp_common.h"

#include "lwip/ip_addr.h"

#define STATIC_STRLEN(x) (sizeof(x) - 1)


int http_system_info_handler(struct query *query)
{
	const char* ptr = query_get_header("Test", query);
	os_printf("user_handler: header test: %s\n", (ptr != NULL ? ptr : "HZ"));
	query_response_status(200, query);

	query_response_header("Content-Type", "application/json", query);

	cJSON *json_root = cJSON_CreateObject();
	cJSON_AddStringToObject(json_root, "action", "test");
	cJSON_AddStringToObject(json_root, "sdk_version", system_get_sdk_version());
	cJSON_AddNumberToObject(json_root, "chip_id", system_get_chip_id());
	cJSON_AddNumberToObject(json_root, "cpu", system_get_cpu_freq());
	cJSON_AddNumberToObject(json_root, "heap_size", system_get_free_heap_size());

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

	if(read_custom_name(&name_info) && read_current_device(&info) && wifi_get_ip_info(SOFTAP_IF, &ip_info))
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
			cJSON_AddStringToObject(json_data, "name", "undefined");
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

int http_get_wifi_info_list_handler(struct query *query)
{
	int result = 0;
	cJSON *json_root = cJSON_CreateObject();
	cJSON *json_data = cJSON_CreateArray();

	uint32_t count = get_wifi_info_list_size();
	os_printf("http: wifi_list size: %d\n", count);

	for(uint32_t i = 0; i < count; ++i, ++result)
	{
		struct wifi_info info;
		memset(&info, 0, sizeof(struct wifi_info));

		if(!read_wifi_info(&info, i))
		{
			result = 0;
			break;
		}

		os_printf("http: ssid: `%s`\n", info.name);

		cJSON *json_value = cJSON_CreateString(info.name);
		cJSON_AddItemToArray(json_data, json_value);
	}
	
	cJSON_AddBoolToObject(json_root, "success", (result ? true : false));
	if(result)
	{	// добавление данных к объекту, json_data удалиться вместе с корневым оюъектом
		cJSON_AddItemToObject(json_root, "data", json_data);
	}
	else
	{	// все плохо удаляем data, так как не прицепилось
		cJSON_Delete(json_data);
	}

	char* data = cJSON_Print(json_root);

	query_response_status(200, query);
	query_response_header("Content-Type", "application/json", query);
	query_response_body(data, strlen(data), query);

	free(data);
	cJSON_Delete(json_root);

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

	return result;
}
