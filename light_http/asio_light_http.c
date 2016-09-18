#include "asio_light_http.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

#include <string.h>
#include <stdlib.h>

#ifndef STOP_TIMER
	/*#define STOP_TIMER 120000*/
	#define STOP_TIMER 10000
#endif

#ifndef CONNECTION_POOL_SIZE
	#define CONNECTION_POOL_SIZE 2
#endif

#define SERVER_VERSION 0.1

/**< Find the maximum of 2 numbers. */
#define max(a,b) ((a)>(b)?(a):(b))  

/**
 * @brief Макрос для подсчета длины const char* во время компиляции
 */
#define STATIC_STRLEN(x) (sizeof(x) - 1)

#ifndef HTTPD_TCP_PRIO
	#define HTTPD_TCP_PRIO TCP_PRIO_MIN
#endif

#ifndef HTTPD_DEBUG
	#define HTTPD_DEBUG LWIP_DBG_ON
	/*#define HTTPD_DEBUG 0xFF*/
#endif

#ifndef HTTPD_MAX_RETRIES
	#define HTTPD_MAX_RETRIES 4
#endif

/** The poll delay is X*500ms */
#ifndef HTTPD_POLL_INTERVAL
	#define HTTPD_POLL_INTERVAL 1
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
static struct http_handler_rule *handlers = NULL;

static struct tcp_pcb* listen_pcb = NULL;

static xQueueHandle webserver_client_task_stop = NULL;
static xQueueHandle socket_queue = NULL;

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

	void* user_data;							///< указатель на пользовательские данные
	response_done_callback after_response;		///< указатель на функцию пост обработки

	char* uri;									///< uri
	uint32_t uri_length;						///< длинная uri
	
	struct query_pair** request_headers;		///< заголовки запроса
	struct query_pair** get_params;				///< параметры из uri
	struct query_pair** post_params;			///< параметры из post

	char* body;									///< тело запроса
	uint32_t body_length;						///< длинна тела запроса

	char* response_body;						///< буфер ответа сервера
	uint32_t response_body_length;				///< текущий размер буфера ответа
	uint32_t response_body_offset;				///< смещение относительно начала response_body, 
												///< используется при отправки данных по частям
	uint32_t done;
};

/**
 * @brief Структура подключения
 */
struct http_ctx
{
	int32_t processed;				///< флаг отвечающий за процессинг
	int32_t retries;				///< количество попыток отправить данные

	struct tcp_pcb* pcb;			///< ассоциированный сокет

	char* receive_buffer;			///< буффер с данными прочитанными из сокета
	int32_t receive_buffer_size;	///< размер буфера с данными

	struct query* query;			///< объект запроса, создается после первичного парсинга запроса иначе NULL
};


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
			os_printf("query_response_append: overflow response buffer size\n");
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
		os_printf("query_response_status: query_response_status invalid query pointer\n");
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

void query_done(struct query* query)
{
	if(query != NULL)
	{
		query->done = true;
	}
}

void query_register_after_response(struct query* query, response_done_callback callback, void* user_data)
{
	if(query != NULL)
	{
		query->after_response = callback;
		query->user_data = user_data;
	}
}


static err_t asio_http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static err_t asio_http_poll(void *arg, struct tcp_pcb *pcb);
static void asio_http_err(void *arg, err_t err);
static err_t asio_http_sent(void *arg, struct tcp_pcb *pcb, u16_t len);

static struct query* init_query()
{
	struct query* query = (struct query*) zalloc(sizeof(struct query));

	query->uri = NULL;
	query->uri_length = 0;

	query->done = false;
	query->user_data = NULL;
	query->after_response = NULL;

	query->body = NULL;
	query->body_length = 0;

	query->request_headers = NULL;
	query->get_params = NULL;
	query->post_params = NULL;

	query->response_body = (char*)zalloc(SEND_BUF_SIZE);
	query->response_body_length = 0;
	query->response_body_offset = 0;

	return query;
}

static void free_query(struct query *query)
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

static struct http_ctx* init_ctx()
{
	struct http_ctx* ctx = (struct http_ctx*) zalloc(sizeof(struct http_ctx));
	ctx->processed = 0;
	ctx->retries = 0;
	ctx->receive_buffer = (char*)zalloc(RECV_BUF_SIZE);
	ctx->receive_buffer_size = RECV_BUF_SIZE;
	return ctx;
}

static void free_ctx(struct http_ctx* ctx)
{
	if(ctx != NULL)
	{
		if(ctx->receive_buffer != NULL)
		{
			free(ctx->receive_buffer);
			ctx->receive_buffer = NULL;
		}

		if(ctx->query != NULL)
		{
			free_query(ctx->query);
		}
		ctx->query = NULL;

		ctx->processed = 0;
		ctx->retries = 0;
		ctx->pcb = NULL;
		free(ctx);
	}
	else
	{
		os_printf("free_ctx: invalid pointer\n");
	}
}

static const struct http_handler_rule* get_handler(struct query *query)
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

static err_t http_close_conn(struct tcp_pcb *pcb, struct http_ctx* ctx)
{
	os_printf("asio_http: Closing connection %p\n", (void*)pcb);

	tcp_arg(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_err(pcb, NULL);
	tcp_poll(pcb, NULL, 0);
	tcp_sent(pcb, NULL);

	err_t err = tcp_close(pcb);
	if (err != ERR_OK) 
	{
		os_printf("asio_http: Error %d closing %p\n", err, (void*)pcb);
		/* error closing, try again later in poll */
		tcp_poll(pcb, asio_http_poll, HTTPD_POLL_INTERVAL);
	}
	else if(ctx != NULL) 
	{
		if(ctx->query->after_response != NULL)
		{
			ctx->query->after_response(ctx->query, ctx->query->user_data);
		}
		free_ctx(ctx);
	}
	return err;
}

static bool webserver_parse_request_headers(struct http_ctx* ctx, char* data)
{
	char *header_name = NULL, *header_value = NULL;
	char *iter = data;

	if(strncmp(iter, "GET ", STATIC_STRLEN("GET ")) == 0)
	{
		ctx->query->method = REQUEST_GET;
		iter += STATIC_STRLEN("GET ");
	}
	else if(strncmp(iter, "POST ", STATIC_STRLEN("POST ")) == 0)
	{
		ctx->query->method = REQUEST_POST;
		iter += STATIC_STRLEN("POST ");
	}
	else
	{
		os_printf("webserver: failed parse method\n");
		if((iter = strstr(iter, " ")) != NULL)
		{
			*iter = '\0';
		}
		os_printf("webserver: unsupported request method: %s\n", 
				(iter != NULL ? ctx->receive_buffer : "unknown"));
		return false;
	}
	os_printf("webserver: method: `%s`\n", (ctx->query->method == REQUEST_GET ? "GET" : "POST"));

	header_name = iter;
	if((iter = strstr(iter, " HTTP/1.1")) == NULL)
	{
		os_printf("webserver: http version tag not found\n");
		return false;
	}
	os_printf("webserver: HTTP tag parsed\n");

	ctx->query->uri = header_name;
	ctx->query->uri_length = iter - header_name;
	*iter = '\0'; ++iter;

	os_printf("webserver: uri: `%s`\n", ctx->query->uri);

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
	os_printf("webserver: headers counted: %d\n", headers_count);

	ctx->query->request_headers = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (headers_count + 1));
	ctx->query->request_headers[headers_count] = NULL;
	for(int32_t i = 0; i < headers_count; ++i)
	{
		ctx->query->request_headers[i] = (struct query_pair*)zalloc(sizeof(struct query_pair));
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
			os_printf("webserver: can't find header separator\n");
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

		ctx->query->request_headers[headers_count]->name = header_name;
		ctx->query->request_headers[headers_count]->value = header_value;
		++headers_count;
	}
	for(struct query_pair** iter = ctx->query->request_headers; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		os_printf("webserver: header name: `%s` - `%s`\n", ptr->name, ptr->value);
	}
	return true;
}

static void webserver_make_param(struct query_pair* pair, char* iter)
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

	os_printf("webserver: name: `%s`, value: `%s`\n", pair->name, pair->value);
}

static bool webserver_parse_params(struct http_ctx* ctx, REQUEST_METHOD m, char* data)
{
	if(data == NULL)
	{
		return true;
	}

	if(m == REQUEST_UNKNOWN)
	{
		os_printf("webserver: params parse target not setted");
		return false;
	}

	char* iter = data, *last = data;
	os_printf("webserver: params data `%s`\n", data);

	// checking tokens
	uint32_t params_counter = (strstr(iter, "=") != NULL ? 1 : 0);
	while((iter = strstr(iter, "&")) != NULL)
	{
		++iter; ++params_counter;
	}

	// empty data, or wrong format
	if(params_counter == 0)
	{
		return true;
	}

	if(m == REQUEST_GET)
	{
		ctx->query->get_params = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (params_counter + 1));
		ctx->query->get_params[params_counter] = NULL;
	}
	else
	{
		ctx->query->post_params = (struct query_pair **) zalloc(sizeof(struct query_pair*) * (params_counter + 1));
		ctx->query->post_params[params_counter] = NULL;
	}

	struct query_pair** target = m == REQUEST_GET 
		? ctx->query->get_params
		: ctx->query->post_params;

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
		++iter;
	}

	/*// not found & token but data not empty*/
	/*if(params_counter == 0)*/
	/*{*/
		webserver_make_param(target[params_counter], last);
	/*}*/

	for(struct query_pair** iter = target; *iter != NULL; ++iter)
	{
		struct query_pair* ptr = *iter;
		os_printf("webserver: params name: `%s` - `%s`\n", ptr->name, ptr->value);
	}
	return true;
}

static bool webserver_parse_request(struct http_ctx* ctx)
{
	if(ctx == NULL || ctx->receive_buffer == NULL)
	{
		return false;
	}

	if(ctx->query == NULL)
	{
		os_printf("webserver: shit happens, query is NULL\n");
		return false;
	}

	char* iter = strstr(ctx->receive_buffer, "\r\n\r\n");
	if(iter != NULL)
	{
		*iter = '\0';
		iter += STATIC_STRLEN("\r\n\r\n");
	}

	if(!webserver_parse_request_headers(ctx, ctx->receive_buffer))
	{
		os_printf("webserver: parse request headers failed\n");
		return false;
	}

	const char* content_length = query_get_header("Content-Length", ctx->query);
	if(content_length != NULL)
	{
		uint32_t header_size = atoi(content_length);
		if(header_size != 0)
		{
			// total size - header_size
			uint32_t body_size = ctx->receive_buffer_size - (iter - ctx->receive_buffer);
			if(body_size != header_size)
			{
				os_printf("webserver: Content-Length: %d, body_size: %d\n", header_size, body_size);
				return false;
			}
		}
		ctx->query->body_length = atoi(content_length);
		ctx->query->body = iter;
	}

	// parse body 
	const char* content_type = query_get_header("Content-Type", ctx->query);
	if(content_type != NULL)
	{
		if(strncmp(content_type, "application/x-www-form-urlencoded", STATIC_STRLEN("application/x-www-form-urlencoded")) != 0)
		{
			os_printf("webserver: Content-Type: `%s` not supported\n", content_type);
		}
		else if(!webserver_parse_params(ctx, REQUEST_POST, ctx->query->body))
		{
			os_printf("webserver: Can't parse params: %s\n", iter);
			return false;
		}
	}

	// parse uri args
	if((iter = strstr(ctx->query->uri, "?")) != NULL)
	{	// remove params from uri
		os_printf("webserver: try parse get %p, %p\n", iter, ctx->query->uri + ctx->query->uri_length);
		*iter = '\0'; ++iter;
		if(!webserver_parse_params(ctx, REQUEST_GET, iter))
		{
			os_printf("webserver: Can't parse params: %s\n", iter);
			return false;
		}
	}

	os_printf("webserver: request parsed\n");
	return true;
}

static err_t http_perform_request(struct http_ctx* ctx)
{
	err_t code = ERR_OK;
	if(ctx != NULL && ctx->receive_buffer != NULL)
	{
		os_printf("http_perform_request: data: %s\n", ctx->receive_buffer);
		if(!webserver_parse_request(ctx))
		{
			query_response_status(500, ctx->query);
		}
		else
		{
			const struct http_handler_rule *handler = get_handler(ctx->query);
			if(handler != NULL && handler->handler != NULL)
			{	
				code = handler->handler(ctx->query) ? ERR_OK : ERR_INPROGRESS;
			}
			else
			{	// не найден обработчик
				query_response_status(404, ctx->query);
			}
		}
	}
	else
	{
		os_printf("webserver: invalid ctx pointer or buffer\n");
		code = ERR_ARG;
	}

	if(code == ERR_OK)
	{
		query_done(ctx->query);
	}
	return code;
}

static uint32_t http_send_data(struct http_ctx* ctx)
{
	if(ctx == NULL)
	{
		os_printf("http_send_data: ctx is null\n");
		return 0;
	}

	if(ctx->query == NULL)
	{
		os_printf("http_send_data: query is null\n");
		return 0;
	}

	uint32_t left_data = ctx->query->response_body_length - ctx->query->response_body_offset;
	os_printf("http_send_data: pcb=%p hs=%p size=%d left=%d offset=%d\n", 
			(void*)ctx->pcb, (void*)ctx, ctx->query->response_body_length, left_data, ctx->query->response_body_offset);

	uint32_t buffer_size = tcp_sndbuf(ctx->pcb) > left_data ? left_data : buffer_size;
	/*uint32_t buffer_size = tcp_sndbuf(ctx->pcb);*/

	os_printf("http_send_data: snd_buff: %d, buff_selected: %d, delta: %d\n", tcp_sndbuf(ctx->pcb), buffer_size, tcp_sndbuf(ctx->pcb) - buffer_size);

	err_t code = tcp_write(ctx->pcb, (const void*)(ctx->query->response_body + ctx->query->response_body_offset), buffer_size, 0);
	if(code == ERR_OK)
	{
		ctx->query->response_body_offset += buffer_size;
		if(ctx->query->response_body_offset >= ctx->query->response_body_length)
		{
			http_close_conn(ctx->pcb, ctx);
		}
	}
	else
	{
		buffer_size = 0;
	}
	return buffer_size;
}

static err_t asio_http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;
	LWIP_UNUSED_ARG(len);

	os_printf("asio_http_sent: %p\n", (void*)pcb);

	if(ctx == NULL) 
	{
		return ERR_OK;
	}

	ctx->retries = 0;
	http_send_data(ctx);

	return ERR_OK;
}

static void asio_http_err(void *arg, err_t err)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;
	LWIP_UNUSED_ARG(err);

	os_printf("http_err: %s", lwip_strerr(err));

	if (ctx != NULL) 
	{
		free_ctx(ctx);
	}
}

static err_t asio_http_poll(void *arg, struct tcp_pcb *pcb)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;

	/*os_printf("asio_http_poll: pcb=%p ctx=%p pcb_state=%s\n",*/
				/*(void*)pcb, (void*)ctx, tcp_debug_state_str(pcb->state));*/

	os_printf("asio_http_poll: pcb=%p ctx=%p\n", (void*)pcb, (void*)ctx);

	if (ctx == NULL) 
	{
		/* arg is null, close. */
		os_printf("asio_http_poll: arg is NULL, close\n");

		err_t closed = http_close_conn(pcb, ctx);
		LWIP_UNUSED_ARG(closed);

		if (closed == ERR_MEM)
		{
			tcp_abort(pcb);
			return ERR_ABRT;
		}
		return ERR_OK;
	} 
	else 
	{
		ctx->retries++;
		if (ctx->retries == HTTPD_MAX_RETRIES) 
		{
			os_printf("asio_http_poll: too many retries, close\n");
			http_close_conn(pcb, ctx);
			return ERR_OK;
		}
	}

	/* If this connection has a file open, try to send some more data. If
	* it has not yet received a GET request, don't do this since it will
	* cause the connection to close immediately. */
	if(ctx && ctx->query != NULL && ctx->query->done) 
	{
		os_printf("asio_http_poll: try to send more data\n");
		if(http_send_data(ctx)) 
		{
			/* If we wrote anything to be sent, go ahead and send it now. */
			os_printf("tcp_output\n");
			tcp_output(pcb);
		}
	}
	return ERR_OK;
}

static err_t asio_http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;

	os_printf("asio_http_recv: pcb=%p pbuf=%p err=%s len=%d tot_len=%d\n", 
				(void*)pcb, (void*)p, lwip_strerr(err), (p!=NULL ? p->len : 0), (p!=NULL ? p->tot_len : 0));

	if ((err != ERR_OK) || (p == NULL) || (ctx == NULL)) 
	{
		/* error or closed by other side? */
		if (p != NULL)
		{
			/* Inform TCP that we have taken the data. */
			tcp_recved(pcb, p->tot_len);
			pbuf_free(p);
		}

		if (ctx == NULL) 
		{
			/* this should not happen, only to be robust */
			os_printf("asio_http_recv: ctx is NULL, close\n");
		}
		http_close_conn(pcb, ctx);
		return ERR_OK;
	}

	if(p->tot_len > RECV_BUF_SIZE)
	{
		os_printf("asio_http_recv: received data so big\n");
		tcp_recved(pcb, p->tot_len);
		pbuf_free(p);
		http_close_conn(pcb, ctx);
		return ERR_OK;
	}

	if (p->len != p->tot_len) 
	{
		os_printf("Warning: incomplete header due to chained pbufs\n");
	}

	if(ctx->query != NULL)
	{
		os_printf("asio_http_recv: received data while request perform\n");
		return ERR_USE;
	}

	if((ctx->query = init_query()) == NULL)
	{
		os_printf("asio_http_recv: init_query failed\n");
		return ERR_MEM;
	}

	// копирование данных для парсинга
	memcpy(ctx->receive_buffer, p->payload, p->len);
	ctx->receive_buffer_size = p->len;
	ctx->pcb = pcb;

	/*portBASE_TYPE call_yield = pdFalse;*/
	/*if(!xQueueSendFromISR(socket_queue, &ctx, &call_yield))*/
	uintptr_t item = (uintptr_t)ctx;
	if(!xQueueSend(socket_queue, &item, STOP_TIMER / portTICK_RATE_MS))
	{
		os_printf("webserver: can't push client ctx to queue\n");

		tcp_recved(pcb, p->tot_len);
		pbuf_free(p);
		http_close_conn(pcb, ctx);
	}
	else
	{
		os_printf("asio_http_recv: pcb tcp_recved\n");
		tcp_recved(pcb, p->tot_len);
		pbuf_free(p);
	}

	/*err_t code = http_perform_request(ctx);*/
	/*if(code == ERR_OK)*/
	/*{*/
	/*}*/
	return ERR_OK;
}

static err_t asio_http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen*)arg;
	LWIP_UNUSED_ARG(err);
	os_printf("http_accept %p / %p\n", (void*)pcb, arg);

	/* Decrease the listen backlog counter */
	tcp_accepted(lpcb);
	/* Set priority */
	tcp_setprio(pcb, HTTPD_TCP_PRIO);

	/* Allocate memory for the structure that holds the state of the
	connection - initialized by that function. */
	struct http_ctx* ctx = init_ctx();
	if (ctx == NULL) 
	{
		os_printf("http_accept: Out of memory, RST\n");
		return ERR_MEM;
	}

	/* Tell TCP that this is the structure we wish to be passed for our
	callbacks. */
	tcp_arg(pcb, ctx);

	/* Set up the various callback functions */
	tcp_recv(pcb, asio_http_recv);
	tcp_err(pcb, asio_http_err);
	tcp_poll(pcb, asio_http_poll, HTTPD_POLL_INTERVAL);
	tcp_sent(pcb, asio_http_sent);

	return ERR_OK;
}

static void asio_init_ctx(ip_addr_t *local_addr)
{
	listen_pcb = tcp_new();
	LWIP_ASSERT("asio_init_ctx: tcp_new failed", listen_pcb != NULL);

	tcp_setprio(listen_pcb, HTTPD_TCP_PRIO);
	/* set SOF_REUSEADDR here to explicitly bind httpd to multiple interfaces */
	err_t err = tcp_bind(listen_pcb, local_addr, WEB_SERVER_PORT);
	LWIP_ASSERT("asio_init_ctx: tcp_bind failed", err == ERR_OK);

	listen_pcb = tcp_listen(listen_pcb);
	LWIP_ASSERT("asio_init_ctx: tcp_listen failed", listen_pcb != NULL);

	/* initialize callback arg and accept callback */
	tcp_arg(listen_pcb, listen_pcb);
	tcp_accept(listen_pcb, asio_http_accept);
}

static bool check_stop_condition(xQueueHandle *queue)
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
		os_printf("webserver: check_stop_condition invalid pointer\n");
	}
	return result;
}

static void webserver_client_task(void *pvParameters)
{
	while(true)
	{
		// checking stop flag
		if(check_stop_condition(&webserver_client_task_stop))
		{
			os_printf("webserver: webserver_client_task rcv exit signal!\n");
			break;
		}

		uintptr_t item = 0;
		if(!xQueueReceive(socket_queue, &item, STOP_TIMER / portTICK_RATE_MS))
		{
			taskYIELD();
		}
		else
		{
			struct http_ctx* ctx = (struct http_ctx*) item;

			os_printf("webserver: ctx ptr: %p\n", (void*)ctx);
			if(ctx != NULL)
			{
				http_perform_request(ctx);
			}
		}
	}
    vQueueDelete(webserver_client_task_stop);
    webserver_client_task_stop = NULL;
    vTaskDelete(NULL);
}


void asio_webserver_start(struct http_handler_rule* user_handlers)
{
	if(handlers == NULL)
	{
		handlers = user_handlers;
	}

    if(webserver_client_task_stop == NULL)
	{
        webserver_client_task_stop = xQueueCreate(1, sizeof(bool));
	}

	if(socket_queue == NULL)
	{
		socket_queue = xQueueCreate(CONNECTION_POOL_SIZE, sizeof(uintptr_t));
	}

	if(listen_pcb == NULL)
	{
		asio_init_ctx(IP_ADDR_ANY);
		xTaskCreate(webserver_client_task, "webserver_client", WEB_HANLDERS_STACK_SIZE, NULL, WEB_HANLDERS_PRIO, NULL);
	}
}

int8_t asio_webserver_stop(void)
{
	if(listen_pcb != NULL)
	{
		tcp_accept(listen_pcb, NULL);
		tcp_close(listen_pcb);
	}
}
