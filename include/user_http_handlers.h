#ifndef __USER_HTTP_HANDLERS_H__
#define __USER_HTTP_HANDLERS_H__

#include "user_config.h"
/*#include "light_http.h"*/
#include "asio_light_http.h"

int http_system_info_handler(struct query *query);
int http_get_device_info_handler(struct query *query);
int http_get_wifi_info_list_handler(struct query *query);
int http_scan_wifi_info_list_handler(struct query *query);

int http_set_device_name_handler(struct query *query);
int http_set_main_wifi_handler(struct query *query);

int http_on_handler(struct query *query);
int http_off_handler(struct query *query);
int http_status_handler(struct query *query);

#endif
