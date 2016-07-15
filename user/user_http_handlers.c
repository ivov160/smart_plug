#include "user_http_handlers.h"
#include "cJSON.h"

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
