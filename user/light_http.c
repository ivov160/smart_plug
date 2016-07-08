#include "light_http.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "json/cJSON.h"

#define DEBUG
#ifdef DEBUG
	#define WS_DEBUG os_printf
#else
	#define WS_DEBUG
#endif

/**
 * @brief Порт для http
 */
#define WEB_SERVER_PORT 80
/**
 * @brief Порт для https
 */
#define WEB_SERVER_SSL_PORT 443

/**
 * @brief Максимальное количество одновременных подключений
 */
#define CONNECTION_POOL_SIZE 1

#define max(a,b) ((a)>(b)?(a):(b))  /**< Find the maximum of 2 numbers. */

/**
 * @brief Время на обработку запроса millisecond
 */
#define STOP_TIMER 120000

/**
 * @brief Размер буфера приемки
 */
#define RECV_BUF_SIZE 2048

#define STATIC_STRLEN(x) (sizeof(x) - 1)

/**
 * @brief Структура для заголовка
 *
 * Поля структуры ссылаются на область памяти используюмую 
 * при чтении данных, таким образом сами поля удалять не надо,
 * буффер чиститься по завершении запроса.
 */
struct query_pair
{
	char* name;
	char* value;
};

/**
 * @brief Объект контекста запроса
 */
struct query
{
	REQUEST_METHOD method;

	char* uri;
	uint32_t uri_length;
	
	struct query_pair** request_headers;
	struct query_pair** params;

	char* body;
	uint32_t body_length;

	short response_status;

	struct query_pair* response_headers;

	char* response_body;
	uint32_t response_body_length;
};

/**
 * @brief Структура подключения
 */
struct connection
{
	int32_t socketfd;				///< сокет хандл клиента
	int32_t timeout;				///< флаг срабатывания таймаута
	int32_t processed;				///< флаг отвечающий за процессинг
	os_timer_t stop_watch;			///< таймер для таймаута

#ifdef SERVER_SSL_ENABLE
    SSL *ssl;						///< ssl ctx
#endif

	char* receive_buffer;			///< буффер с данными прочитанными из сокета
	int32_t receive_buffer_size;	///< размер буфера с данными

	struct query* query;			///< объект запроса, создается после первичного парсинга запроса иначе NULL
};

/**
 * @brief Структура пула коннектов
 *
 */
struct connections_pool 
{
	int32_t size;
	struct connection** data;
};

LOCAL struct connections_pool connections;
LOCAL struct connection* connection_list[CONNECTION_POOL_SIZE];

LOCAL xQueueHandle QueueStop = NULL;
LOCAL xQueueHandle RCVQueueStop = NULL;

/**
 * @brief Метод для создания объекта запроса
 *
 * Создает объект запроса и иницилизирует поля 
 * значениями по умолчанию
 *
 */
LOCAL void init_query(struct connection *connection)
{
	if(connection->query == NULL)
	{
        connection->query = (struct query *)zalloc(sizeof(struct query));

		connection->query->uri = NULL;
		connection->query->uri_length = 0;

		connection->query->body = NULL;
		connection->query->body_length = 0;

		connection->query->response_status = 404;

		connection->query->response_body = NULL;
		connection->query->response_body_length = 0;

		connection->query->request_headers = NULL;
		connection->query->response_headers = NULL;
	}
}

/**
 * @brief Метод для разрушения query
 */
LOCAL void cleanup_query(struct query *query)
{
	if(query != NULL)
	{
		for(struct query_pair** iter = query->request_headers; *iter != NULL; ++iter)
		{
			free((struct query_pair*)(*iter));
		}

		for(struct query_pair** iter = query->params; *iter != NULL; ++iter)
		{
			free((struct query_pair*)(*iter));
		}

		free(query->params);
		free(query->request_headers);
		free(query->response_body);
		free(query->response_headers);
		free(query);
	}
}

/**
 * @brief Инициализация пулла коннектов
 */
LOCAL void connection_pool_init(void)
{
    for(uint8_t index = 0; index < CONNECTION_POOL_SIZE; index++)
	{
        connection_list[index] = (struct connection *)zalloc(sizeof(struct connection));
        connection_list[index]->socketfd = -1;
        connection_list[index]->timeout =  0;
        connection_list[index]->processed =  0;
		connection_list[index]->query = NULL;
		connection_list[index]->receive_buffer = NULL;
    }

	// current usage connections
    connections.size = 0; 
    connections.data = connection_list;

    WS_DEBUG("C > multi_conn_init ok!\n");
}

/**
 * @brief Связывание сокета с неиспользуемым коннектом в пуле
 */
LOCAL void accept_connection(uint8_t index, struct connections_pool* pool, int32_t remotefd)
{
	if(pool != NULL && index < CONNECTION_POOL_SIZE)
	{
		pool->size++;
		pool->data[index]->socketfd = remotefd;
		pool->data[index]->receive_buffer = NULL;
		pool->data[index]->query = NULL;
	}
	else
	{
		WS_DEBUG("webserver: accept_connection invalid index: %i\n", index);
	}
}

/**
 * @brief Закрытие соеденения с клиентом
 * 
 * Метод закрывает сокет и вычищает данные?
 */
LOCAL void close_connection(uint8_t index, struct connections_pool* pool)
{
	if(pool != NULL && index < pool->size)
	{
#ifdef SERVER_SSL_ENABLE
		ssl_free(pool->data[index]->ssl);
		pool->data[index]->ssl = NULL;
#endif
		close(pool->data[index]->socketfd);

		if(pool->data[index]->receive_buffer != NULL)
		{
			free(pool->data[index]->receive_buffer);
			pool->data[index]->receive_buffer = NULL;
		}

		if(pool->data[index]->query != NULL)
		{
			cleanup_query(pool->data[index]->query);
		}
		pool->data[index]->query = NULL;

		pool->data[index]->processed = 0;
		pool->data[index]->timeout = 0;
		pool->data[index]->socketfd = -1;

		pool->size--;
	}
	else
	{
		WS_DEBUG("webserver: close_connection invalid index: %i\n", index);
	}
}

/**
 * @brief Метод для завершения обработки запроса
 *
 * Метод маркирует connection для последующего закрытия и 'отправки данных?'
 *
 */
LOCAL void finalize_connection(struct connection* connection)
{
	connection->processed = 1;
}

char* query_get_header(const char* name, struct query* query)
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

char* query_get_param(const char* name, struct query* query)
{
	if(query == NULL)
	{
		return NULL;
	}

	for(struct query_pair** iter = query->params; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		if(strcmp(ptr->name, name) == 0)
		{
			return ptr->value;
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

char* query_get_uri(struct query* query)
{
	if(query == NULL)
	{
		return NULL;
	}
	return query->uri;
}

/**
 * @brief Обработчик таймаута обработки подключения
 */
LOCAL void webserver_conn_watcher(struct connection* pconnection)
{
    os_timer_disarm(&pconnection->stop_watch);
    pconnection->timeout = 1;
    
    WS_DEBUG("webserver: watcher sock_fd %d timeout!\n", pconnection->socketfd);
}

/**
 * @brief Функция для разбора заголовков запроса
 * @todo Описать как парсит
 * @return true удалось распарсить заголовки иначе false
 */
bool webserver_parse_request_headers(struct connection* connection, char* data)
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
	uint8_t headers_count = 0;
	while((iter = strstr(iter, "\r\n")) != NULL)
	{
		iter += STATIC_STRLEN("\r\n");
		++headers_count;
	}
	WS_DEBUG("webserver: headers counted: %d\n", headers_count);

	connection->query->request_headers = (struct query_pair **) zalloc(sizeof(struct query_pair*) * headers_count);
	connection->query->request_headers[headers_count] = NULL;
	for(uint8_t i = 0; i < headers_count; ++i)
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

void webserver_make_param(struct query_pair* pair, char* iter)
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

bool webserver_parse_params(struct connection* connection, char* data)
{
	if(data == NULL)
	{
		return true;
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

	connection->query->params = (struct query_pair **) zalloc(sizeof(struct query_pair*) * params_counter);
	connection->query->params[params_counter] = NULL;
	for(uint32_t i = 0; i < params_counter; ++i)
	{
		connection->query->params[i] = (struct query_pair*)zalloc(sizeof(struct query_pair));
	}

	iter = last;
	params_counter = 0;
	while(*iter != '\0')
	{
		if(*iter == '&')
		{
			*iter = '\0';
			webserver_make_param(connection->query->params[params_counter], last);
			last = (iter + 1);

			++params_counter;
		}
		++iter; ;
	}

	// not found & token but data not empty
	if(params_counter == 0)
	{
		webserver_make_param(connection->query->params[params_counter], last);
	}

	for(struct query_pair** iter = connection->query->params; *iter != NULL; ++iter)
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

	if(connection->query != NULL)
	{
		WS_DEBUG("webserver: shit happens, query is not NULL\n");
		return false;
	}
	init_query(connection);

	char* iter = strstr(connection->receive_buffer, "\r\n\r\n");
	if(iter != NULL)
	{
		*iter = '\0';
		iter += STATIC_STRLEN("\r\n\r\n");
	}

	WS_DEBUG("webserver: before parse headers\n");
	if(!webserver_parse_request_headers(connection, connection->receive_buffer))
	{
		WS_DEBUG("webserver: parse request headers failed\n");
		return false;
	}

	char* content_length = query_get_header("Content-Length", connection->query);
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

	char* content_type = query_get_header("Content-Type", connection->query);
	if(content_type != NULL)
	{
		if(strncmp(content_type, "application/x-www-form-urlencoded", STATIC_STRLEN("application/x-www-form-urlencoded")) != 0)
		{
			WS_DEBUG("webserver: Content-Type: `%s` not supported\n", content_type); 
		}
		else if(!webserver_parse_params(connection, connection->query->body))
		{
			WS_DEBUG("webserver: Can't parse params: %s\n", iter);
			return false;
		}
	}
	else
	{
		iter = strstr(connection->query->uri, "?");
		if(iter != NULL)
		{	// remove params from uri
			*iter = '\0'; ++iter;
			if(!webserver_parse_params(connection, iter))
			{
				WS_DEBUG("webserver: Can't parse params: %s\n", iter);
				return false;
			}
		}
	}
	WS_DEBUG("webserver: request parsed\n");
	return true;
}

LOCAL void webserver_recvdata_process(struct connection *connection)
{
	if(connection != NULL && connection->receive_buffer != NULL)
	{
		WS_DEBUG("webserver: data: %s\n", connection->receive_buffer);
		if(!webserver_parse_request(connection))
		{
			WS_DEBUG("webserver: can't parse request, socket: %d", connection->socketfd);
		}
		///@todo implement call user handler
		else
		{
			char* ptr = query_get_header("Test", connection->query);
			WS_DEBUG("webserver: header test: %s\n", (ptr != NULL ? ptr : "HZ"));
		}
		finalize_connection(connection);
	}
	else
	{
		WS_DEBUG("webserver: invalid connection pointer or buffer");
	}
}

LOCAL void webserver_recv_thread(void *pvParameters)
{
    int stack_counter = 0;
    struct connections_pool* pconnection_pool = (struct connections_pool*) pvParameters;

#ifdef SERVER_SSL_ENABLE
    uint8_t  quiet = FALSE;
    uint8_t *read_buf = NULL;
    SSL_CTX *ssl_ctx = NULL;
    
    if ((ssl_ctx = ssl_ctx_new(SSL_DISPLAY_CERTS, SSL_DEFAULT_SVR_SESS)) == NULL) 
	{
        WS_DEBUG("Error: Server context is invalid\n");
    }
#else 
    char *precvbuf = (char*)malloc(RECV_BUF_SIZE);
    if(NULL == precvbuf)
	{
        WS_DEBUG("webserver: recv_thread, memory exhausted, check it\n");
    }
#endif

    uint32_t maxfdp = 0;
    while(1)
	{
		bool ValueFromReceive = FALSE;
		portBASE_TYPE xStatus = xQueueReceive(RCVQueueStop, &ValueFromReceive, 0);
        if ( pdPASS == xStatus && TRUE == ValueFromReceive)
		{
            WS_DEBUG("webserver: webserver_recv_thread rcv exit signal!\n");
            break;
        }

        while(pconnection_pool->size == 0)
		{
            vTaskDelay(1000 / portTICK_RATE_MS);
            /*if no client coming in, wait in big loop*/
            continue;
        }

        /*clear fdset, and set the selct function wait time*/
		fd_set readset;
        FD_ZERO(&readset);

        for(uint8_t index = 0; index < CONNECTION_POOL_SIZE; index++)
		{ //find all valid handle 
            if(pconnection_pool->data[index]->socketfd >= 0)
			{
                FD_SET(pconnection_pool->data[index]->socketfd, &readset);
                maxfdp = max(pconnection_pool->data[index]->socketfd, maxfdp);
				WS_DEBUG("webserver: selected index: %d\n", index);
            }
        }

        //polling all exist client handle
		struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(maxfdp + 1, &readset, NULL, NULL, &timeout);
        if(ret > 0)
		{
#ifdef SERVER_SSL_ENABLE
            for(uint8_t index = 0; index < CONNECTION_POOL_SIZE; index++)
			{	/* IF this handle there is data/event aviliable, recive it*/
                if (FD_ISSET(pconnection_pool->data[index]->socketfd, &readset))
                {	/*stop the sock handle watchout timer */
                    os_timer_disarm((os_timer_t *)&pconnection_pool->data[index]->stop_watch);
                
                    if(NULL == pconnection_pool->data[index]->ssl)
					{
                        pconnection_pool->data[index]->ssl = ssl_server_new(ssl_ctx, pconnection_pool->data[index]->socketfd);
                    }
                    
                    if ((ret = ssl_read(pconnection_pool->data[index]->ssl, &read_buf)) == SSL_OK) 
					{	/* in the middle of doing a handshake */
                        if (ssl_handshake_status(pconnection_pool->single_conn[index]->ssl) == SSL_OK) 
						{
                            if (!quiet) 
							{
                                display_session_id(pconnection_pool->data[index]->ssl);
                                display_cipher(pconnection_pool->data[index]->ssl);
                                WS_DEBUG("webserver: connection handshake ok!\n");
                                quiet = true;
                            }
                        }
                    }
                    
                    if (ret > SSL_OK) 
					{  
                        WS_DEBUG("webserver: webserver_recv_thread recv and process sockfd %d!\n", pconnection_pool->data[index]->socketfd);
						pconnection_pool->data[index]->receive_buffer = read_buf;
						pconnection_pool->data[index]->receive_buffer_size = ret;
						webserver_recvdata_process(pconnection_pool->data[index]);

                        /*restart the sock handle watchout timer */
                        os_timer_setfn((os_timer_t *)&pconnection_pool->data[index]->stop_watch, (os_timer_func_t *)webserver_conn_watcher, pconnection_pool->data[index]);
                        os_timer_arm((os_timer_t *)&pconnection_pool->data[index]->stop_watch, STOP_TIMER, 0);
                        
                    } 
					else if (ret == SSL_CLOSE_NOTIFY) 
					{
                        WS_DEBUG("webserver: shutting down SSL\n");
                        
                    } 
					else if (ret < SSL_OK)
					{
                        WS_DEBUG("webserver: webserver_recv_thread CONNECTION CLOSED index %d !\n", index);
						close_connection(index, pconnection_pool);
                    }
                }
                else if(pconnection_pool->data[index]->timeout == 1)
				{ /* IF this handle there is no data/event aviliable, check the timeout flag*/
                    WS_DEBUG("webserver: webserver_recv_thread index %d timeout,close!\n",index);
					close_connection(index, pconnection_pool);
                }
            }
#else
            for(uint8_t index = 0; index < CONNECTION_POOL_SIZE; index++)
			{	/* IF this handle there is data/event aviliable, recive it*/
                if (FD_ISSET(pconnection_pool->data[index]->socketfd, &readset))
                {	/*stop the sock handle watchout timer */
                    os_timer_disarm((os_timer_t *)&pconnection_pool->data[index]->stop_watch);

					// bind buffer for selected connection
                    memset(precvbuf, 0, RECV_BUF_SIZE);

                    ret = recv(pconnection_pool->data[index]->socketfd, precvbuf, RECV_BUF_SIZE, 0);
                    if(ret > 0)
					{
                        WS_DEBUG("webserver: webserver recv sockfd %d\n", pconnection_pool->data[index]->socketfd);
					
						pconnection_pool->data[index]->receive_buffer = precvbuf;
						pconnection_pool->data[index]->receive_buffer_size = ret;
						webserver_recvdata_process(pconnection_pool->data[index]);

                        /*restart the sock handle watchout timer */
                        os_timer_setfn((os_timer_t *)&pconnection_pool->data[index]->stop_watch, (os_timer_func_t *)webserver_conn_watcher, pconnection_pool->data[index]);
                        os_timer_arm((os_timer_t *)&pconnection_pool->data[index]->stop_watch, STOP_TIMER, 0);
                    }
					else
					{	//recv error,connection close
                        WS_DEBUG("webserver: close sockfd %d !\n", pconnection_pool->data[index]->socketfd);
						close_connection(index, pconnection_pool);
                    }
                }

				WS_DEBUG("webserver: index: %d, timeout: %d, processed: %d\n", index, pconnection_pool->data[index]->timeout, pconnection_pool->data[index]->processed);
				if(pconnection_pool->data[index]->timeout)
				{
					WS_DEBUG("webserver: close connection by timeout\n");
					close_connection(index, pconnection_pool);
				}
				else if(pconnection_pool->data[index]->processed)
				{
					WS_DEBUG("webserver: close processed connection\n");
					close_connection(index, pconnection_pool);
				}
            }
#endif
        }
		else if(ret == -1)
		{ //select timerout out, wait again. 
            WS_DEBUG("webserver: ##WS select timeout##\n");
        }
        
        /*for develop test only*/
        if(stack_counter++ == 1)
		{
            stack_counter = 0;
            WS_DEBUG("webserver: webserver_recv_thread %d word left\n", uxTaskGetStackHighWaterMark(NULL));
        }
    }

    for(uint8_t index = 0; index < CONNECTION_POOL_SIZE && pconnection_pool->size; index++)
	{
        //find all valid handle 
        if(pconnection_pool->data[index]->socketfd >= 0)
		{
            os_timer_disarm((os_timer_t *)&pconnection_pool->data[index]->stop_watch);
			close_connection(index, pconnection_pool);
        }
    }
    
#ifdef SERVER_SSL_ENABLE
    ssl_ctx_free(ssl_ctx);
#endif

    if(precvbuf != NULL)
	{
        free(precvbuf);
    }

    vQueueDelete(RCVQueueStop);
    RCVQueueStop = NULL;
    vTaskDelete(NULL);
}

void webserver_recv_task_start(struct connections_pool* pconnections)
{
    if (RCVQueueStop == NULL)
	{
        RCVQueueStop = xQueueCreate(1,1);
	}
    
    if (RCVQueueStop != NULL)
	{	///@todo read about task memory
        sys_thread_new("websrecv_thread", webserver_recv_thread, pconnections, 512, 6);//1024, 704 left 320 used
    }
}

int8_t webserver_recv_task_stop(void)
{
    bool ValueToSend = true;
    portBASE_TYPE xStatus;
    if (RCVQueueStop == NULL)
	{
        return -1;
	}

    xStatus = xQueueSend(RCVQueueStop,&ValueToSend,0);
    if (xStatus != pdPASS)
	{
        WS_DEBUG("WEB SEVER Could not send to the rcvqueue!\n");
        return -1;
    } 
	else 
	{
        taskYIELD();
        return pdPASS;
    }
}


LOCAL void webserver_task(void *pvParameters)
{
    int32_t listenfd;
    int32_t len;
    int32_t ret;
    uint8_t index;
    
    struct ip_info ipconfig;
    struct connections_pool* pconnection_pool;
    struct sockaddr_in server_addr, remote_addr;

    portBASE_TYPE xStatus;
    bool ValueFromReceive = false;

    int stack_counter = 0;
    
    /* Construct local address structure */
    memset(&server_addr, 0, sizeof(server_addr)); /* Zero out structure */
    server_addr.sin_family = AF_INET;            /* Internet address family */
    server_addr.sin_addr.s_addr = INADDR_ANY;   /* Any incoming interface */
    server_addr.sin_len = sizeof(server_addr);  
#ifdef SERVER_SSL_ENABLE
    server_addr.sin_port = htons(WEB_SERVER_SSL_PORT); /* Local SSL port */
#else
    server_addr.sin_port = htons(WEB_SERVER_PORT); /* Local port */
#endif

    /* Create socket for incoming connections */
    do
	{
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd == -1) 
		{
            WS_DEBUG("C > user_webserver_task failed to create sock!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } 
	while(listenfd == -1);

    /* Bind to the local address */
    do
	{
        ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret != 0) 
		{
            WS_DEBUG("C > user_webserver_task failed to bind sock!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
	while(ret != 0);

    do
	{
        /* Listen to the local connection */
        ret = listen(listenfd, CONNECTION_POOL_SIZE);
        if (ret != 0) 
		{
            WS_DEBUG("C > user_webserver_task failed to set listen queue!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
	while(ret != 0);
    
    /*initialize as connections set*/
    connection_pool_init();
    pconnection_pool = &connections;
    
    /*start a task to recv data from client*/
	webserver_recv_task_start(pconnection_pool);

	while(1)
    {
		// checking stop flag
		xStatus = xQueueReceive(QueueStop, &ValueFromReceive, 0);
		if (pdPASS == xStatus && TRUE == ValueFromReceive)
		{
			WS_DEBUG("user_webserver_task rcv exit signal!\n");
			break;
		}

		/*block here waiting remote connect request*/
		len = sizeof(struct sockaddr_in);
		int32_t remotefd = accept(listenfd, (struct sockaddr *)&remote_addr, (socklen_t *)&len);
		if(remotefd != -1)
		{
			//find the fisrt usable connections param to save the handle.
			for(index = 0; index < CONNECTION_POOL_SIZE; index++)
			{
				if(pconnection_pool->data[index]->socketfd < 0)
				{
					break;
				}
			}

			if(index < CONNECTION_POOL_SIZE)
			{
				accept_connection(index, pconnection_pool, remotefd);

				// установка таймаута на обработку запроса
				os_timer_disarm(&pconnection_pool->data[index]->stop_watch);
				os_timer_setfn(&pconnection_pool->data[index]->stop_watch, (os_timer_func_t *)webserver_conn_watcher, pconnection_pool->data[index]);
				os_timer_arm(&pconnection_pool->data[index]->stop_watch, STOP_TIMER, 0);

				WS_DEBUG("WEB SERVER acpt index:%d sockfd %d!\n",index, remotefd);
			}
			else
			{
				close(remotefd);
				WS_DEBUG("WEB SERVER TOO MUCH CONNECTION, CHECK ITer!\n");
			}
		}
		else
		{
			WS_DEBUG("WEB SERVER remote error: %d, WARNING!\n", remotefd);
		}

		///@todo remove debug info when failed accept connection
		if(stack_counter++ == 1)
		{
			stack_counter = 0;
			WS_DEBUG("user_webserver_task %d word left\n", uxTaskGetStackHighWaterMark(NULL));
		}
    }
	
	// stop recv task
	webserver_recv_task_stop();
    
    close(listenfd);
    vQueueDelete(QueueStop);
    QueueStop = NULL;
    vTaskDelete(NULL);
}

void webserver_start(void)
{
    if(QueueStop == NULL)
	{
        QueueStop = xQueueCreate(1, 1);
	}

    if(QueueStop != NULL)
	{	///@todo read about task memory
        xTaskCreate(webserver_task, "webserver", 280, NULL, 4, NULL); //512, 376 left,136 used
	}
}

int8_t webserver_stop(void)
{
    bool ValueToSend = true;
    portBASE_TYPE xStatus;
    if (QueueStop == NULL)
	{
        return -1;
	}

    xStatus = xQueueSend(QueueStop, &ValueToSend, 0);
    if (xStatus != pdPASS)
	{
        WS_DEBUG("WEB SEVER Could not send to the queue!\n");
        return -1;
    } 
	else
	{
        taskYIELD();
        return pdPASS;
    }
}


