#ifndef __USER_HTTP_HANDLERS_H__
#define __USER_HTTP_HANDLERS_H__

#include "../light_http/light_http.h"

/**
 * @defgroup user User 
 * @defgroup user_light_http User light_http
 * @brief Пользовательский код для light_http
 *
 * @addtogroup user
 * @{
 * @addtogroup user_light_http 
 * @{
 */

int http_system_info_handler(struct query *query);
int http_get_device_info_handler(struct query *query);
int http_scan_wifi_info_list_handler(struct query *query);

int http_set_device_name_handler(struct query *query);
int http_set_main_wifi_handler(struct query *query);
int http_get_wifi_error_handler(struct query *query);

int http_on_handler(struct query *query);
int http_off_handler(struct query *query);
int http_status_handler(struct query *query);

int http_start_test_mode(struct query *query);
int http_stop_test_mode(struct query *query);

/**
 * @}
 * @}
 */

#endif
