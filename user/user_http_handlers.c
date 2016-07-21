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
	cJSON *json_root = cJSON_CreateObject();

	if(read_current_device(&info) && wifi_get_ip_info(SOFTAP_IF, &ip_info))
	{
		cJSON *json_data = cJSON_CreateObject();
		cJSON_AddItemToObject(json_root, "data", json_data);

		char ip_print_buffer[4 * 4 + 1] = { 0 };
		sprintf(ip_print_buffer, IPSTR, IP2STR(&ip_info.ip));

		cJSON_AddNumberToObject(json_data, "powered", info.device_type >> 7);
		cJSON_AddNumberToObject(json_data, "type", info.device_type & 0x0F);
		cJSON_AddStringToObject(json_data, "name", "unknown");
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

	uint32_t wifi_index = 0;
	while(wifi_index < WIFI_LIST_SIZE)
	{
		struct wifi_info info;
		if(!read_wifi_info(&info, wifi_index))
		{
			result = 0;
			break;
		}

		if(strlen(info.name) == 0)
		{	// возможно конец списка (не дошли до граници, но данных нет)
			/*break;*/
			continue;
		}
		os_printf("http: ssid: `%s`\n", info.name);

		cJSON *json_value = cJSON_CreateString(info.name);
		cJSON_AddItemToArray(json_data, json_value);
		++wifi_index;
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
