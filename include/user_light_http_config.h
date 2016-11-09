#ifndef __USER_LIGHT_HTTP_CONFIG_H__
#define __USER_LIGHT_HTTP_CONFIG_H__

#include "esp_common.h"
#include "user_config.h"

/**
 * @defgroup user User 
 * @defgroup user_light_http User light_http
 *
 * @addtogroup user
 * @{
 * @addtogroup user_light_http 
 * @{
 */

#define RECV_BUF_SIZE 1024
#define SEND_BUF_SIZE 1024 * 2
#define CONNECTION_POOL_SIZE 2
#define HTTPD_TCP_PRIO DEFAULT_TASK_PRIO + 1
#define WEB_HANLDERS_PRIO DEFAULT_TASK_PRIO + 2

/**
 * @}
 * @}
 */

#endif
