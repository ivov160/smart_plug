#ifndef __USER_LIGHT_HTTP_H__
#define __USER_LIGHT_HTTP_H__

#include "esp_common.h"
#include "esp_libc.h"

/**
 * @brief Контекст запроса
 */
struct query;

/** 
 * @brief Сигнатура обработчика запроса
 * Возвращает статус ответа ?
 */
typedef int (* cgi_handler)(struct query *query);

/**
 * @brief Правило обработки урла
 */
struct http_handler_rule
{
	const char* uri;		///< url для обработки (доступные маски ???)
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
char* query_get_header(const char* name, struct query* query);

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
char* query_get_param(const char* name, struct query* query);

/**
 * @brief Метод для получения параметров запроса
 * @param[in] query Указатель на запрос
 */
char* query_get_uri(struct query* query);

/**
 * @brief Метод для выставления статуса ответа
 * @param[in] status Статус ответа
 * @param[in] query Указатель на запрос
 */
void query_set_status(short status, struct query* query);

/**
 * @brief Метод для выставления заголовка ответа
 * @param[in] status Статус ответа
 * @param[in] query Указатель на запрос
 */
void query_set_header(const char* name, const char* value);

/**
 * @brief Метод для установки тела ответа
 * @param[in] data Тело ответа
 * @param[in] length Размер тела
 * @param[in] query Указатель на запрос
 */
void query_set_body(const char* data, uint32_t length, struct query* query);

/**
 * @brief Метод для запуска http сервера
 */
void webserver_start(void);

/**
 * @brief Метод для остановки http сервера
 */
int8_t webserver_stop(void);

#endif

