#include "user_http_handlers.h"

int test_http_handler(struct query *query)
{
	const char* ptr = query_get_header("Test", query);
	os_printf("user_handler: header test: %s\n", (ptr != NULL ? ptr : "HZ"));
	query_response_status(200, query);

	char buff[PRINT_BUFFER_SIZE] = { 0 };
	uint32_t size = sprintf(buff, "<html><head></head><body><h1>Header: %s</h1></body></html>", ptr);
	query_response_body(buff, size, query);

	return 1;
}
