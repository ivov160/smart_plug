#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include "esp_common.h"

/**
 * @defgroup user User 
 * @brief Пользовательский код
 *
 * @addtogroup user
 * @{
 */

// gdb config
#define GDBSTUB_FREERTOS 1
//#define GDBSTUB_USE_OWN_STACK 1

// default prio for tasks
#define DEFAULT_TASK_PRIO tskIDLE_PRIORITY

/**
 * @brief Максимальное кол-во подключений к устройству в режиме AP
 */
#define MAX_AP_CONNECTION 10

/**
 * @brief Длинна дла генерируемого пароля
 * @note Так же добавляется как часть имени сети
 */
#define PASSWORD_LENGTH 8

/**
 * @brief Максимальная длинна ssid
 */
#define MAX_SSID_LENGTH 32

/**
 * @brief Размер стека для event_loop
 */
#define WIFI_EVENT_HANDLER_STACK_SIZE 128

/**
 * @brief Приоритет для event_loop
 */
#define WIFI_EVENT_HANDLER_PRIO DEFAULT_TASK_PRIO 

/**
 * @brief Время ожидания нового события при опросе очереди
 */
#define WIFI_EVENT_WAIT_TIMER 10000

/**
 * @brief Размер очереди event_loop
 */
#define WIFI_EVENT_POOL_SIZE 10

/**
 * @brief Таймаут для таймера проверки подключения к wifi точке доступа
 * Данный таймаут используется внутри wifi_set_station_info для
 * проверки соеденения к wifi сети. Если по истечению данного времени 
 * был зарегистрирован disconect от ожидаемой точки доступа. То произойдет
 * возврат к режиму AP c сохраннными ранее настройками.
 */
#define WIFI_STATION_CHECK_TIMER 5000

/**
 * @}
 */

#endif
