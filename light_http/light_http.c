#include "light_http.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define DEBUG
#ifdef DEBUG
	#define WS_DEBUG os_printf
#else
	#define WS_DEBUG
#endif

/**
 * @brief версия сервера
 */
#define SERVER_VERSION 0.1

/**< Find the maximum of 2 numbers. */
#define max(a,b) ((a)>(b)?(a):(b))  

/**
 * @brief Макрос для подсчета длины const char* во время компиляции
 */
#define STATIC_STRLEN(x) (sizeof(x) - 1)

/**
 * @brief Обрабочики запросов
 * Инициализируется при старте web сервера
 */
LOCAL struct http_handler_rule *handlers = NULL;

/**
 * @brief Структура для заголовка
 *
 * Поля структуры ссылаются на область памяти используюмую 
 * при чтении данных, таким образом сами поля удалять не надо,
 * буффер чиститься по завершении запроса.
 */
struct query_pair
{
	char* name;			///< ключ
	char* value;		///< значение
};

/**
 * @brief Объект контекста запроса
 */
struct query
{
	REQUEST_METHOD method;						///< Тип запроса

	char* uri;									///< uri
	uint32_t uri_length;						///< длинная uri
	
	struct query_pair** request_headers;		///< заголовки запроса
	struct query_pair** get_params;				///< параметры из uri
	struct query_pair** post_params;			///< параметры из post

	char* body;									///< тело запроса
	uint32_t body_length;						///< длинна тела запроса

	char* response_body;						///< буфер ответа сервера
	uint32_t response_body_length;				///< текущий размер буфера ответа
};

/**
 * @brief Структура подключения
 */
struct connection
{
	int32_t socketfd;				///< сокет хандл клиента
	int32_t timeout;				///< флаг срабатывания таймаута
	int32_t processed;				///< флаг отвечающий за процессинг
	/*os_timer_t stop_watch;			///< таймер для таймаута*/

	char* receive_buffer;			///< буффер с данными прочитанными из сокета
	int32_t receive_buffer_size;	///< размер буфера с данными

	struct query* query;			///< объект запроса, создается после первичного парсинга запроса иначе NULL
};

///@todo replace for event or somthing else using queu mb very fat
/// stop condition
LOCAL xQueueHandle webserver_task_stop = NULL;
LOCAL xQueueHandle webserver_client_task_stop = NULL;
/// queu for client sockets
LOCAL xQueueHandle socket_queue = NULL;

LOCAL void webserver_conn_watcher(struct connection* connection);

/**
 * @brief Функция для поиска обработчика запроса
 * @return указатель на обработчик, NULL если ненашлось
 */
LOCAL const struct http_handler_rule* get_handler(struct query *query)
{
	struct http_handler_rule* handler = NULL;
	if(handlers != NULL)
	{
		handler = handlers;
		const char* uri = query_get_uri(query);
		while(handler != NULL && handler->uri != NULL && handler->handler != NULL)
		{
			if(strcmp(uri, handler->uri) == 0)
			{
				break;
			}
			++handler;
		}
		// checking end of handlers list
		if(handler->uri == NULL || handler->handler == NULL)
		{
			handler = NULL;
		}
	}
	return handler;
}

const char* query_get_header(const char* name, struct query* query)
{
	if(query == NULL)
	{
		return NULL;
	}

	for(struct query_pair** iter = query->request_headers; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		if(strcmp(ptr->name, name) == 0)
		{
			return ptr->value;
		}
	}
	return NULL;
}

const char* query_get_param(const char* name, struct query* query, REQUEST_METHOD m)
{
	if(query == NULL)
	{
		return NULL;
	}

	if(m == REQUEST_UNKNOWN)
	{
		return NULL;
	}

	struct query_pair** target = m == REQUEST_GET 
		? query->get_params
		: query->post_params;

	if(target != NULL)
	{
		for(struct query_pair** iter = target; *iter != NULL; ++iter)
		{
			struct query_pair* ptr = *iter;
			if(strcmp(ptr->name, name) == 0)
			{
				return ptr->value;
			}
		}
	}
	return NULL;
}

REQUEST_METHOD query_get_method(struct query* query)
{
	if(query == NULL)
	{
		return REQUEST_UNKNOWN;
	}
	return query->method;
}

const char* query_get_uri(struct query* query)
{
	if(query == NULL)
	{
		return NULL;
	}
	return query->uri;
}

LOCAL bool query_response_append(const char* data, uint32_t size, struct query* query)
{
	if(query != NULL)
	{
		if(query->response_body_length + size >= SEND_BUF_SIZE)
		{
			WS_DEBUG("webserver: overflow response buffer size\n");
			return false;
		}

		// append data to buffer
		// buffer already allocated
		memcpy(query->response_body + query->response_body_length, data, size);
		query->response_body_length += size;
	}
	return true;
}

void query_response_status(int32_t status, struct query* query)
{
	if(query != NULL)
	{
		char buff[PRINT_BUFFER_SIZE] = { 0 };
		int size = sprintf(buff, 
				"HTTP/1.1 %d OK\r\n"
				"Server: light-httpd/0.1\r\n"
				"Connection: close\r\n", status);

		query_response_append(buff, size, query);
	}
	else
	{
		WS_DEBUG("webserver: query_response_status invalid query pointer\n");
	}
}

void query_response_header(const char* name, const char* value, struct query* query)
{
	if(query != NULL)
	{
		char buff[PRINT_BUFFER_SIZE] = { 0 };
		int32_t size = sprintf(buff, "%s: %s\r\n", name, value);
		query_response_append(buff, size, query);
	}
}

void query_response_body(const char* data, int32_t length, struct query* query)
{
	if(query != NULL)
	{
		char buff[PRINT_BUFFER_SIZE] = { 0 };
		int32_t size = sprintf(buff, "Content-Length: %u\r\n", length);
		query_response_append(buff, size, query);

		query_response_append("\r\n", STATIC_STRLEN("\r\n"), query);
		query_response_append(data, length, query);
	}
}

/**
 * @brief Метод для создания объекта запроса
 *
 * Создает объект запроса и иницилизирует поля 
 * значениями по умолчанию
 *
 */
LOCAL struct query* init_query()
{
	struct query* query = (struct query *)zalloc(sizeof(struct query));

	query->uri = NULL;
	query->uri_length = 0;

	query->body = NULL;
	query->body_length = 0;

	query->request_headers = NULL;
	query->get_params = NULL;
	query->post_params = NULL;

	query->response_body = (char*)zalloc(SEND_BUF_SIZE);
	query->response_body_length = 0;

	return query;
}

/**
 * @brief Метод для разрушения query
 */
LOCAL void cleanup_query(struct query *query)
{
	if(query != NULL)
	{
		if(query->request_headers != NULL)
		{
			for(struct query_pair** iter = query->request_headers; *iter != NULL; ++iter)
			{
				free((struct query_pair*)(*iter));
			}
			free(query->request_headers);
		}

		if(query->get_params != NULL)
		{
			for(struct query_pair** iter = query->get_params; *iter != NULL; ++iter)
			{
				free((struct query_pair*)(*iter));
			}
			free(query->get_params);
		}

		if(query->post_params != NULL)
		{
			for(struct query_pair** iter = query->post_params; *iter != NULL; ++iter)
			{
				free((struct query_pair*)(*iter));
			}
			free(query->post_params);
		}

		if(query->response_body != NULL)
		{
			free(query->response_body);
		}
		free(query);
	}
}

/**
 * @brief Метод для завершения обработки запроса
 *
 * Метод маркирует connection для последующего завершения
 *
 */
LOCAL void finalize_connection(struct connection* connection)
{
	connection->processed = 1;
}

/**
 * @brief Инициирование нового подключения
 */
LOCAL struct connection* init_connection(int32_t socketfd)
{
	struct connection *connection = (struct connection*)zalloc(sizeof(struct connection));
	connection->socketfd = socketfd;
	connection->timeout = 0;
	connection->processed = 0;
	connection->receive_buffer = (char*)zalloc(RECV_BUF_SIZE);
	connection->receive_buffer_size = RECV_BUF_SIZE;
	connection->query = init_query();
	return connection;
}

/**
 * @brief Метод для разрушения подключения
 */
LOCAL void cleanup_connection(struct connection *connection)
{
	if(connection != NULL)
	{
		if(connection->receive_buffer != NULL)
		{
			free(connection->receive_buffer);
			connection->receive_buffer = NULL;
		}

		if(connection->query != NULL)
		{
			cleanup_query(connection->query);
		}
		connection->query = NULL;

		connection->processed = 0;
		connection->timeout = 0;
		connection->socketfd = -1;
	}
	else
	{
		WS_DEBUG("webserver: cleanup_connection invalid pointer\n");
	}
}

/**
 * @brief Закрытие соеденения с клиентом
 * Метод закрывает сокет
 */
LOCAL void close_connection(struct connection *connection)
{
	if(connection != NULL)
	{
		close(connection->socketfd);
	}
	else
	{
		WS_DEBUG("webserver: close_connection invalid pointer\n");
	}
}

/**
 * @brief Метод для остановки таймера ожидания 
 */
LOCAL void stop_timer_connection(struct connection *connection)
{
	if(connection != NULL)
	{
		/*os_timer_disarm(&connection->stop_watch);*/
	}
	else
	{
		WS_DEBUG("webserver: stop_timer_connection invalid pointer\n");
	}
}

/**
 * @brief Метод запускает таймер для ожидания
 *
 * Используется для реалицазии таймаута при записи или чтении
 */
LOCAL void start_timer_connection(struct connection *connection)
{
	if(connection != NULL)
	{
		/*stop_timer_connection(connection);*/
		/*os_timer_setfn(&connection->stop_watch, (os_timer_func_t *)webserver_conn_watcher, connection);*/
		/*os_timer_arm(&connection->stop_watch, STOP_TIMER, 0);*/
	}
	else
	{
		WS_DEBUG("webserver: start_timer_connection invalid pointer\n");
	}
}

/**
 * @brief Метод для отправки данных
 */
LOCAL uint32_t connection_send_data(struct connection* connection)
{
	uint32_t writed = 0;
	if(connection != NULL)
	{
		if(connection->query != NULL)
		{
			/*stop_timer_connection(connection);*/
			// запуск таймера, если не успеем отправить ответ, то сокет закроеться
			/*start_timer_connection(connection);*/
			if (connection->query->response_body_length != 0)
			{
				writed = write(connection->socketfd, (uint8_t*)connection->query->response_body, connection->query->response_body_length);
				/*writed = write(connection->socketfd, (uint8_t*)"123123", STATIC_STRLEN("123123"));*/
				connection->query->response_body_length = 0;
			}
			/*stop_timer_connection(connection);*/
		}
	}
	return writed;
}

/**
 * @brief Обработчик таймаута обработки подключения
 */
LOCAL void webserver_conn_watcher(struct connection *connection)
{
	/*os_timer_disarm(&connection->stop_watch);*/
	connection->timeout = 1;
	close_connection(connection);

	WS_DEBUG("webserver: watcher sock_fd %d timeout!\n", connection->socketfd);
}

/**
 * @brief Функция для разбора заголовков запроса
 * @todo Описать как парсит
 * @return true удалось распарсить заголовки иначе false
 */
LOCAL bool webserver_parse_request_headers(struct connection* connection, char* data)
{
	char *header_name = NULL, *header_value = NULL;
	char *iter = data;

	if(strncmp(iter, "GET ", STATIC_STRLEN("GET ")) == 0)
	{
		connection->query->method = REQUEST_GET;
		iter += STATIC_STRLEN("GET ");
	}
	else if(strncmp(iter, "POST ", STATIC_STRLEN("POST ")) == 0)
	{
		connection->query->method = REQUEST_POST;
		iter += STATIC_STRLEN("POST ");
	}
	else
	{
		WS_DEBUG("webserver: failed parse method\n");
		if((iter = strstr(iter, " ")) != NULL)
		{
			*iter = '\0';
		}
		WS_DEBUG("webserver: unsupported request method: %s\n", 
				(iter != NULL ? connection->receive_buffer : "unknown"));
		return false;
	}
	WS_DEBUG("webserver: method: `%s`\n", (connection->query->method == REQUEST_GET ? "GET" : "POST"));

	header_name = iter;
	if((iter = strstr(iter, " HTTP/1.1")) == NULL)
	{
		WS_DEBUG("webserver: http version tag not found\n");
		return false;
	}
	WS_DEBUG("webserver: HTTP tag parsed\n");

	connection->query->uri = header_name;
	connection->query->uri_length = iter - header_name;
	*iter = '\0'; ++iter;

	WS_DEBUG("webserver: uri: `%s`\n", connection->query->uri);

	// safe position when using
	// it afte headers_count calculation
	header_name = iter;

	// calculate headers count
	// count > size by 1 because calculated method row
	// but count usage for headers array ending with NULL
	int32_t headers_count = 0;
	while((iter = strstr(iter, "\r\n")) != NULL)
	{
		iter += STATIC_STRLEN("\r\n");
		++headers_count;
	}
	WS_DEBUG("webserver: headers counted: %d\n", headers_count);

	connection->query->request_headers = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (headers_count + 1));
	connection->query->request_headers[headers_count] = NULL;
	for(int32_t i = 0; i < headers_count; ++i)
	{
		connection->query->request_headers[i] = (struct query_pair*)zalloc(sizeof(struct query_pair));
	}

	// restore saved position
	iter = header_name;
	headers_count = 0;
	while((iter = strstr(iter, "\r\n")) != NULL)
	{
		// set stop value for header_value 
		// from prev iteration
		if(headers_count != 0)
		{
			*iter = '\0';
		}

		iter += STATIC_STRLEN("\r\n");
		header_name = iter;

		if((iter = strstr(iter, ":")) == NULL)
		{
			WS_DEBUG("webserver: can't find header separator\n");
			continue;
		}

		// set header_name stop value
		// end go to next char
		*iter = '\0'; ++iter;
		while(*iter == ' ' || *iter == '\t')
		{
			++iter;
		}
		header_value = iter;

		connection->query->request_headers[headers_count]->name = header_name;
		connection->query->request_headers[headers_count]->value = header_value;
		++headers_count;
	}
	for(struct query_pair** iter = connection->query->request_headers; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		WS_DEBUG("webserver: header name: `%s` - `%s`\n", ptr->name, ptr->value);
	}
	return true;
}

LOCAL void webserver_make_param(struct query_pair* pair, char* iter)
{
	char* ptr = strstr(iter, "=");
	if(ptr == NULL)
	{
		pair->name = iter;
		pair->value = &iter[strlen(iter)];
	}
	else
	{
		*ptr = '\0';
		pair->name = iter;
		pair->value = (ptr + 1);
	}
}

LOCAL bool webserver_parse_params(struct connection* connection, REQUEST_METHOD m, char* data)
{
	if(data == NULL)
	{
		return true;
	}

	if(m == REQUEST_UNKNOWN)
	{
		WS_DEBUG("webserver: params parse target not setted");
		return false;
	}

	char* iter = data, *last = data;
	WS_DEBUG("webserver: params data `%s`\n", data);

	// checking tokens
	uint32_t params_counter = (strstr(iter, "=") != NULL ? 1 : 0);
	while((iter = strstr(iter, "&")) != NULL)
	{
		++iter; ++params_counter;
	}

	// empty data, or wrong format
	if(params_counter == 0)
	{
		return false;
	}

	if(m == REQUEST_GET)
	{
		connection->query->get_params = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (params_counter + 1));
		connection->query->get_params[params_counter] = NULL;
	}
	else
	{
		connection->query->post_params = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (params_counter + 1));
		connection->query->post_params[params_counter] = NULL;
	}

	struct query_pair** target = m == REQUEST_GET 
		? connection->query->get_params
		: connection->query->post_params;

	for(uint32_t i = 0; i < params_counter; ++i)
	{
		target[i] = (struct query_pair*)zalloc(sizeof(struct query_pair));
	}

	iter = last;
	params_counter = 0;
	while(*iter != '\0')
	{
		if(*iter == '&')
		{
			*iter = '\0';
			webserver_make_param(target[params_counter], last);
			last = (iter + 1);

			++params_counter;
		}
		++iter; ;
	}

	// not found & token but data not empty
	if(params_counter == 0)
	{
		webserver_make_param(target[params_counter], last);
	}

	for(struct query_pair** iter = target; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		WS_DEBUG("webserver: params name: `%s` - `%s`\n", ptr->name, ptr->value);
	}
	return true;
}

/**
 * @brief Метод для разбора данных
 *
 * Метод разбирает данные из соеденения и формирует структуру query
 *
 * @return true если данные успешно распаршены, иначе false
 */
LOCAL bool webserver_parse_request(struct connection *connection)
{
	if(connection == NULL || connection->receive_buffer == NULL)
	{
		return false;
	}

	if(connection->query == NULL)
	{
		WS_DEBUG("webserver: shit happens, query is NULL\n");
		return false;
	}

	char* iter = strstr(connection->receive_buffer, "\r\n\r\n");
	if(iter != NULL)
	{
		*iter = '\0';
		iter += STATIC_STRLEN("\r\n\r\n");
	}

	if(!webserver_parse_request_headers(connection, connection->receive_buffer))
	{
		WS_DEBUG("webserver: parse request headers failed\n");
		return false;
	}

	const char* content_length = query_get_header("Content-Length", connection->query);
	if(content_length != NULL)
	{
		uint32_t header_size = atoi(content_length);
		if(header_size != 0)
		{
			// total size - header_size
			uint32_t body_size = connection->receive_buffer_size - (iter - connection->receive_buffer);
			if(body_size != header_size)
			{
				WS_DEBUG("webserver: Content-Length: %d, body_size: %d\n", header_size, body_size);
				return false;
			}
		}
		connection->query->body_length = atoi(content_length);
		connection->query->body = iter;
	}

	// parse body 
	const char* content_type = query_get_header("Content-Type", connection->query);
	if(content_type != NULL)
	{
		if(strncmp(content_type, "application/x-www-form-urlencoded", STATIC_STRLEN("application/x-www-form-urlencoded")) != 0)
		{
			WS_DEBUG("webserver: Content-Type: `%s` not supported\n", content_type); 
		}
		else if(!webserver_parse_params(connection, REQUEST_POST, connection->query->body))
		{
			WS_DEBUG("webserver: Can't parse params: %s\n", iter);
			return false;
		}
	}

	// parse uri args
	iter = strstr(connection->query->uri, "?");
	if(iter != NULL)
	{	// remove params from uri
		*iter = '\0'; ++iter;
		if(!webserver_parse_params(connection, REQUEST_GET, iter))
		{
			WS_DEBUG("webserver: Can't parse params: %s\n", iter);
			return false;
		}
	}

	WS_DEBUG("webserver: request parsed\n");
	return true;
}

/** 
 * @brief Серверный обработчик подключения
 */
LOCAL void webserver_connection_handle(struct connection *connection)
{
	if(connection != NULL && connection->receive_buffer != NULL)
	{
		WS_DEBUG("webserver: data: %s\n", connection->receive_buffer);
		if(!webserver_parse_request(connection))
		{
			WS_DEBUG("webserver: can't parse request, socket: %d \n", connection->socketfd);
			query_response_status(500, connection->query);
		}
		else
		{
			const struct http_handler_rule *handler = get_handler(connection->query);
			if(handler != NULL && handler->handler != NULL)
			{	
				handler->handler(connection->query);
			}
			else
			{	// не найден обработчик
				query_response_status(404, connection->query);
			}
		}
		finalize_connection(connection);
	}
	else
	{
		WS_DEBUG("webserver: invalid connection pointer or buffer\n");
	}
}

/**
 * @brief Функция для проверки стоп флага
 */
LOCAL bool check_stop_condition(xQueueHandle *queue)
{
	bool result = true;
	if(queue != NULL)
	{
		portBASE_TYPE xStatus = xQueueReceive(*queue, &result, 0);
		if (pdPASS == xStatus && result == TRUE)
		{
			result = true;
		}
		else
		{
			result = false;
		}
	}
	else
	{
		WS_DEBUG("webserver: check_stop_condition invalid pointer\n");
	}
	return result;
}

LOCAL int32_t webserver_start_listen()
{
	///@todo add checking stop condition for task
	int32_t listen_socket = 0;
    do
	{	// Create socket for incoming connections
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket == -1) 
		{
            WS_DEBUG("webserver: failed to create sock!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } 
	while(listen_socket == -1);
    
	int32_t result;
    do
	{	// Bind to the local address
		struct sockaddr_in server_addr;

		/* Construct local address structure */
		memset(&server_addr, 0, sizeof(server_addr)); /* Zero out structure */
		server_addr.sin_family = AF_INET;            /* Internet address family */
		server_addr.sin_addr.s_addr = INADDR_ANY;   /* Any incoming interface */
		server_addr.sin_len = sizeof(server_addr);  
		server_addr.sin_port = htons(WEB_SERVER_PORT); /* Local port */

        result = bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (result != 0) 
		{
            WS_DEBUG("webserver: failed to bind sock!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
	while(result != 0);

    do
	{	// Listen to the local connection */
        result = listen(listen_socket, CONNECTION_POOL_SIZE);
        if (result != 0) 
		{
            WS_DEBUG("webserver: failed to set listen queue!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
	while(result != 0);
	return listen_socket;
}

LOCAL void webserver_client_task(void *pvParameters)
{
	while(true)
	{
		// checking stop flag
		if(check_stop_condition(&webserver_client_task_stop))
		{
			WS_DEBUG("webserver: webserver_client_task rcv exit signal!\n");
			break;
		}

		int32_t client_socket = 0;
		if(!xQueueReceive(socket_queue, &client_socket, STOP_TIMER / portTICK_RATE_MS))
		{
			taskYIELD();
		}
		else
		{
			struct connection *connection = init_connection(client_socket);
			if(connection == NULL)
			{
				WS_DEBUG("webserver: webserver_client_task, memory exhausted, check it\n");
				close(client_socket);

				// next iteration
				continue;
			}

			// запуск таймера для чтения
			/*start_timer_connection(connection);*/
			int32_t result = recv(connection->socketfd, connection->receive_buffer, connection->receive_buffer_size, 0);
			/*stop_timer_connection(connection);*/

			if(result > 0)
			{
				WS_DEBUG("webserver: webserver recv sockfd %d\n", connection->socketfd);

				// store real read size
				connection->receive_buffer_size = result;
				webserver_connection_handle(connection);

				WS_DEBUG("webserver: timeout: %d, processed: %d\n", connection->timeout, connection->processed);
				if(connection->timeout == 1)
				{	// connection already closed
					WS_DEBUG("webserver: connection timeout detected!\n");
				}
				else if(connection->processed == 1)
				{
					WS_DEBUG("webserver: close processed connection\n");
					connection_send_data(connection);
				}
			}
			else if(connection->timeout == 1)
			{	// connection already closed
				WS_DEBUG("webserver: connection timeout detected!\n");
			}
			else if(connection->timeout = 0)
			{	//recv error,connection close
				WS_DEBUG("webserver: close sockfd %d !\n", connection->socketfd);
			}
			close_connection(connection);
			cleanup_connection(connection);
		}
	}
    vQueueDelete(webserver_client_task_stop);
    webserver_client_task_stop = NULL;
    vTaskDelete(NULL);
}

LOCAL void webserver_task(void *pvParameters)
{
	WS_DEBUG("webserver: started, STOP_TIMER: %d, RECV_BUF_SIZE: %d, SEND_BUF_SIZE: %d, PRINT_BUFFER_SIZE: %d, CONNECTION_POOL_SIZE: %d\n",
			STOP_TIMER, RECV_BUF_SIZE, SEND_BUF_SIZE, PRINT_BUFFER_SIZE, CONNECTION_POOL_SIZE);

	int32_t listen_socket = webserver_start_listen();
	while(true)
    {
		// checking stop flag
		if(check_stop_condition(&webserver_task_stop))
		{
			WS_DEBUG("webserver: webserver_task rcv exit signal!\n");
			break;
		}

		struct sockaddr_in remote_addr;
		uint32_t len = sizeof(struct sockaddr_in);

		/*block here waiting remote connect request*/
		int32_t remotefd = accept(listen_socket, (struct sockaddr *)&remote_addr, (socklen_t *)&len);
		if(remotefd != -1)
		{
			if(!xQueueSend(socket_queue, &remotefd, STOP_TIMER / portTICK_RATE_MS))
			{
				close(remotefd);
				WS_DEBUG("webserver: can't push client socket to queue\n");
			}
		}
		else
		{
			WS_DEBUG("webserver: accept failed error: %d\n", remotefd);
		}
    }
    close(listen_socket);

    vQueueDelete(webserver_task_stop);
    webserver_task_stop = NULL;
	handlers = NULL;
    vTaskDelete(NULL);
}

void webserver_start(struct http_handler_rule *user_handlers)
{
	if(handlers == NULL)
	{
		handlers = user_handlers;
	}

    if(webserver_task_stop == NULL)
	{
        webserver_task_stop = xQueueCreate(1, sizeof(bool));
	}

    if(webserver_client_task_stop == NULL)
	{
        webserver_client_task_stop = xQueueCreate(1, sizeof(bool));
	}

	if(socket_queue == NULL)
	{
		socket_queue = xQueueCreate(CONNECTION_POOL_SIZE, sizeof(int32_t));
	}

    if(webserver_task_stop != NULL && socket_queue != NULL)
	{	///@todo read about task memory
        xTaskCreate(webserver_task, "webserver", 280, NULL, WEB_CONNECTION_PRIO, NULL); //512, 376 left,136 used
		xTaskCreate(webserver_client_task, "webserver_client", 280, NULL, WEB_HANLDERS_PRIO, NULL);
	}
}

int8_t webserver_stop(void)
{
	int8_t result = 0;
    if (webserver_task_stop == NULL || webserver_client_task_stop == NULL)
	{
        result -1;
	}
	else
	{
		bool ValueToSend = true;
		portBASE_TYPE xStatus = xQueueSend(webserver_task_stop, &ValueToSend, 0);
		if (xStatus != pdPASS)
		{
			WS_DEBUG("webserver: Could not send stop to webserver_task\n");
		} 
		else
		{
			taskYIELD();
			result = pdPASS;
		}

		xStatus = xQueueSend(webserver_client_task_stop, &ValueToSend, 0);
		if (xStatus != pdPASS)
		{
			WS_DEBUG("webserver: Could not send stop to webserver_client_task\n");
		} 
		else
		{
			taskYIELD();
			result = pdPASS;
		}
	}
	return result;
}


