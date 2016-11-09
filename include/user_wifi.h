#ifndef __USER_WIFI_STATION_H__
#define __USER_WIFI_STATION_H__

#include "../data/data.h"
#include "user_config.h"

/**
 * @defgroup user User 
 * @defgroup user_wifi User wifi
 * @brief Пользовательский код для wifi
 *
 * @addtogroup user
 * @{
 * @addtogroup user_wifi
 * @{
 */

/**
 * @brief Функция для установки настроек wifi подключения
 * @param[in] info Информация о wifi сети
 * @param[in] connect Подключиться к новой сети, true - да
 * @return результат работы true - все получилось
 */
bool wifi_set_station_info(struct data_wifi_info* info, bool connect);

/** 
 * @brief Получить последнюю ошибку disconnect
 * Ошибка сохраняется если событие EVENT_STAMODE_DISCONNECTED
 * полученно для сети, к которой была попытка подключения
 */
const char* wifi_get_last_error();

/**
 * @brief Функция для старта точки доступа
 * @param[in] info Информация о текущем устройстве
 * @return результат запуска true - все получилось
 */
bool wifi_start_ap(struct data_device_info* info);

/**
 * @brief ФУнкция дла запуска wifi в режиме клиента
 *
 * @note Параметр connect используется как костыль
 * для настройки wifi в user_init. Так как после завершения
 * user_init происходит autoconnect по выставленным настройкам.
 *
 * @param[in] info Информация по wifi точке доступа
 * @param[in] connect Подключаться сразу или нет
 * @return результат запуска true - все получилось
 */
bool wifi_start_station(struct data_wifi_info* info, bool connect);

/**
 * @brief Функция остановки работы wifi
 * @param[in] cleanup Указывает на остановку event_loop и отчистку всех его частей
 */
void wifi_stop(bool cleanup);

/**
 * @brief Функция для получения ip по текущему wifi соеденению
 * @param[out] ip_info Структура для заполнения информацией
 * @return Результат операции true - данные удалось извлечь
 */
bool wifi_get_ip(struct ip_info* ip_info);

/**
 * @}
 * @}
 */

#endif

