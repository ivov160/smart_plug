#ifndef __USER_LIGHT_HTTP_H__
#define __USER_LIGHT_HTTP_H__

#include "esp_common.h"
#include "esp_libc.h"
#include "user_light_http_config.h"
#include "light_http_config.h"

/**
 * @defgroup light_http 
 * @brief HTTP сервер
 *
 * @addtogroup light_http
 * @{
 */

/**
 * @brief Контекст запроса
 */
struct query;

/** 
 * @brief Сигнатура обработчика запроса
 *
 * Ответ указывает на формат ответа: 
 * 1 - синхронный ответ
 * 0 - асинхронный ответ
 *
 * Синхроннвый ответ - запрос помечается как обработанный и при первем же
 * вызове poll, данные будут отправленны, а соеденение закрыто
 *
 * Асинхроннвый ответ - запрос остается в работе и
 * в каждом вызове poll проверяется его завершенность,
 * как только запрос будет помечен обработанным, запрос финализируется.
 * Если завпрос в течении отведенного кол-ва попыток (HTTPD_MAX_RETRIES)
 * не будет обработан, соеденение разрывается.
 *
 * @see query_done
 */
typedef int (* cgi_handler)(struct query *query);

/**
 * @brief Сигнатура callback вызываемого перед разрушением query
 */
typedef void (* response_done_callback)(struct query *query, void* user_data);

/**
 * @brief Правило обработки урла
 * @note В данный момент точное совпадение path (strcmp)
 * @todo Добавить маски в uri
 */
struct http_handler_rule
{
	const char* uri;		///< url для обработки 
	cgi_handler handler;	///< функция для обработки запроса
};

/**
 * @brief Типы заросов
 */
typedef enum 
{
	REQUEST_GET = 0,
	REQUEST_POST = 1,
	REQUEST_UNKNOWN = 99
} REQUEST_METHOD;

/**
 * @brief Метод для получения заголовка
 * @param[in] name Имя заголовка
 * @param[in] query Указатель на запрос
 */
const char* query_get_header(const char* name, struct query* query);

/**
 * @brief Метод для получения метода запроса
 * @param[in] query Указатель на запрос
 */
REQUEST_METHOD query_get_method(struct query* query);

/**
 * @brief Метод для получения параметров запроса
 * @param[in] name Имя параметра
 * @param[in] query Указатель на запрос
 * @param[in] param_type Тип параметра (GET, POST)
 */
const char* query_get_param(const char* name, struct query* query, REQUEST_METHOD m);

/**
 * @brief Метод для получения параметров запроса
 * @param[in] query Указатель на запрос
 */
const char* query_get_uri(struct query* query);

/**
 * @brief Метод для отправки статуса ответа
 * @param[in] status Статус ответа
 * @param[in] query Указатель на запрос
 */
void query_response_status(int32_t status, struct query* query);

/**
 * @brief Метод для отправки заголовка ответа
 * @param[in] status Статус ответа
 * @param[in] query Указатель на запрос
 */
void query_response_header(const char* name, const char* value, struct query* query);

/**
 * @brief Метод для отправки тела ответа
 * @param[in] data Тело ответа
 * @param[in] length Размер тела
 * @param[in] query Указатель на запрос
 */
void query_response_body(const char* data, int32_t length, struct query* query);

/**
 * @brief Функция помечает запрос как обработанный
 *
 * Если запрос помечен как обработанный, то происходит
 * автоматическая отправка данных клиенты и закрытие соеденения
 *
 * @param[in] query Указатель на запрос
 */
void query_done(struct query* query);

/**
 * @brief Функция регистрирует постобработку
 * @param[in] query Указатель на запрос
 * @param[in] user_data Указатель на пользовательские данные
 */
void query_register_after_response(struct query* query, response_done_callback callback, void* user_data);

/**
 * @brief Метод для запуска сервера
 * @param handlers Список обработчиков
 */
void asio_webserver_start(struct http_handler_rule *handlers);

/**
 * @brief Метод для остановки http сервера
 */
int8_t asio_webserver_stop(void);

/**
 * @}
 */


#endif
