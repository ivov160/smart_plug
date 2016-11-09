#ifndef __LIGHT_HTTP_CONFIG_H__
#define __LIGHT_HTTP_CONFIG_H__

/**
 * @defgroup light_http 
 * @brief HTTP сервер
 *
 * @addtogroup light_http
 * @{
 */

/**
 * @brief Время на обработку запроса millisecond
 */
#ifndef STOP_TIMER
	#define STOP_TIMER 10000
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
 * @brief Максимальное количество одновременных подключений
 */
#ifndef CONNECTION_POOL_SIZE
	#define CONNECTION_POOL_SIZE 2
#endif

/**
 * @brief Количество вызовов poll, 
 * используется для ограничкния времени обработки запроса
 */
#ifndef HTTPD_MAX_RETRIES
	#define HTTPD_MAX_RETRIES 15
#endif

/**
 * @brief Таймаут между вызовами pool
 * общая задержка X*500ms
 * * X - HTTPD_POLL_INTERVAL
 * * 500ms - LwIP constant
 */
#ifndef HTTPD_POLL_INTERVAL
	#define HTTPD_POLL_INTERVAL 1
#endif

/**
 * @brief Приоритет потока обработки tcp соеденений (LwIP)
 *
 */
#ifndef HTTPD_TCP_PRIO
	#define HTTPD_TCP_PRIO TCP_PRIO_MIN
#endif

/**
 * @brief Размер стека таска выполняющего пользовательские обработчики
 */
#ifndef WEB_HANLDERS_STACK_SIZE 
	#define WEB_HANLDERS_STACK_SIZE (SEND_BUF_SIZE + RECV_BUF_SIZE + 2 * 1024) / 4
#endif

/**
 * @brief Приоритет таска выполнающим пользовательские обработчики
 *
 */
#ifndef WEB_HANLDERS_PRIO 
	#define WEB_HANLDERS_PRIO HTTPD_TCP_PRIO
#endif

/**
 * @}
 */


#endif
