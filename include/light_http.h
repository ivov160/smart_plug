#ifndef __USER_LIGHT_HTTP_H__
#define __USER_LIGHT_HTTP_H__

#include "esp_common.h"
#include "esp_libc.h"

#include "light_http_config.h"

/**
 * @brief Время на обработку запроса millisecond
 */
#ifndef STOP_TIMER
	#define STOP_TIMER 120000
#endif

/**
 * @brief Размер буфера приемки
 */
#ifndef RECV_BUF_SIZE
	#define RECV_BUF_SIZE 2048
#endif

/**
 * @brief Размер буфера отправки
 */
#ifndef SEND_BUF_SIZE
	#define SEND_BUF_SIZE 2048
#endif

/**
 * @brief размер буфера для рендера частей ответа сервера
 */
#ifndef PRINT_BUFFER_SIZE
	#define PRINT_BUFFER_SIZE 256
#endif

/**
 * @brief Порт для http
 */
#ifndef WEB_SERVER_PORT
	#define WEB_SERVER_PORT 80
#endif

/**
 * @brief Порт для https
 */
#ifndef WEB_SERVER_SSL_PORT
	#define WEB_SERVER_SSL_PORT 443
#endif

/**
 * @brief Максимальное количество одновременных подключений
 */
#ifndef CONNECTION_POOL_SIZE
	#define CONNECTION_POOL_SIZE 2
#endif

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
void query_response_status(short status, struct query* query);

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
void query_response_body(const char* data, uint32_t length, struct query* query);

/**
 * @brief Метод для запуска http сервера
 */
void webserver_start(void);

/**
 * @brief Метод для остановки http сервера
 */
int8_t webserver_stop(void);

#endif

