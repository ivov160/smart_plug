#include "asio_light_http.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

#include <string.h>
#include <stdlib.h>

#define DEBUG
#ifdef DEBUG
	#define WS_DEBUG os_printf
#else
	#define WS_DEBUG
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
	#define HTTPD_DEBUG LWIP_DBG_OFF
#endif

#ifndef HTTPD_MAX_RETRIES
	#define HTTPD_MAX_RETRIES 4
#endif

/** The poll delay is X*500ms */
#ifndef HTTPD_POLL_INTERVAL
	#define HTTPD_POLL_INTERVAL 4
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
struct http_ctx
{
	int32_t processed;				///< флаг отвечающий за процессинг
	int32_t retries;				///< количество попыток отправить данные

	struct tcp_pcb* pcb;			///< ассоциированный сокет

	char* receive_buffer;			///< буффер с данными прочитанными из сокета
	int32_t receive_buffer_size;	///< размер буфера с данными

	struct query* query;			///< объект запроса, создается после первичного парсинга запроса иначе NULL
};


static err_t asio_http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static err_t asio_http_poll(void *arg, struct tcp_pcb *pcb);
static void asio_http_err(void *arg, err_t err);
static err_t asio_http_sent(void *arg, struct tcp_pcb *pcb, u16_t len);

static struct query* init_query()
{
	struct query* query = (struct query*) zalloc(sizeof(struct query));

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
	}
	else
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("free_ctx: invalid pointer\n"));
	}
}

static err_t http_close_conn(struct tcp_pcb *pcb, struct http_ctx* ctx)
{
	LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http: Closing connection %p\n", (void*)pcb));

	tcp_arg(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_err(pcb, NULL);
	tcp_poll(pcb, NULL, 0);
	tcp_sent(pcb, NULL);

	if(ctx != NULL) 
	{
		free_ctx(ctx);
	}

	err_t err = tcp_close(pcb);
	if (err != ERR_OK) 
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http: Error %d closing %p\n", err, (void*)pcb));
		/* error closing, try again later in poll */
		tcp_poll(pcb, asio_http_poll, HTTPD_POLL_INTERVAL);
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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: failed parse method\n"));
		if((iter = strstr(iter, " ")) != NULL)
		{
			*iter = '\0';
		}
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: unsupported request method: %s\n", 
				(iter != NULL ? ctx->receive_buffer : "unknown")));
		return false;
	}
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: method: `%s`\n", 
					(ctx->query->method == REQUEST_GET ? "GET" : "POST")));

	header_name = iter;
	if((iter = strstr(iter, " HTTP/1.1")) == NULL)
	{
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: http version tag not found\n"));
		return false;
	}
	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: HTTP tag parsed\n"));

	ctx->query->uri = header_name;
	ctx->query->uri_length = iter - header_name;
	*iter = '\0'; ++iter;

	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: uri: `%s`\n", ctx->query->uri));

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
	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: headers counted: %d\n", headers_count));

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
			LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: can't find header separator\n"));
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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: header name: `%s` - `%s`\n", ptr->name, ptr->value));
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
}

static bool webserver_parse_params(struct http_ctx* ctx, REQUEST_METHOD m, char* data)
{
	if(data == NULL)
	{
		return true;
	}

	if(m == REQUEST_UNKNOWN)
	{
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: params parse target not setted"));
		return false;
	}

	char* iter = data, *last = data;
	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: params data `%s`\n", data));

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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: params name: `%s` - `%s`\n", ptr->name, ptr->value));
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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: shit happens, query is NULL\n"));
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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: parse request headers failed\n"));
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
				LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: Content-Length: %d, body_size: %d\n", 
							header_size, body_size));
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
			LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: Content-Type: `%s` not supported\n", content_type));
		}
		else if(!webserver_parse_params(ctx, REQUEST_POST, ctx->query->body))
		{
			LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: Can't parse params: %s\n", iter));
			return false;
		}
	}

	// parse uri args
	iter = strstr(ctx->query->uri, "?");
	if(iter != NULL)
	{	// remove params from uri
		*iter = '\0'; ++iter;
		if(!webserver_parse_params(ctx, REQUEST_GET, iter))
		{
			LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: Can't parse params: %s\n", iter));
			return false;
		}
	}

	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: request parsed\n"));
	return true;
}

static err_t http_perform_request(struct http_ctx* ctx)
{
	err_t code = ERR_OK;
	if(ctx != NULL && ctx->receive_buffer != NULL)
	{
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_perform_request: data: %s\n", ctx->receive_buffer));
		if(!webserver_parse_request(ctx))
		{
			/*WS_DEBUG("webserver: can't parse request, socket: %d \n", ctx->socketfd);*/
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
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("webserver: invalid ctx pointer or buffer\n"));
		code = ERR_ARG;
	}
	return code;
}

static uint32_t http_send_data(struct http_ctx* ctx)
{
}

static err_t asio_http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;
	LWIP_UNUSED_ARG(len);

	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("asio_http_sent: %p\n", (void*)pcb));

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

	LWIP_DEBUGF(HTTPD_DEBUG, ("http_err: %s", lwip_strerr(err)));

	if (ctx != NULL) 
	{
		free_ctx(ctx);
	}
}

static err_t asio_http_poll(void *arg, struct tcp_pcb *pcb)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;

	LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("asio_http_poll: pcb=%p hs=%p pcb_state=%s\n", 
				(void*)pcb, (void*)ctx, tcp_debug_state_str(pcb->state)));

	if (ctx == NULL) 
	{
		/* arg is null, close. */
		LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_poll: arg is NULL, close\n"));

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
			LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_poll: too many retries, close\n"));
			http_close_conn(pcb, ctx);
			return ERR_OK;
		}
	}

	/* If this connection has a file open, try to send some more data. If
	* it has not yet received a GET request, don't do this since it will
	* cause the connection to close immediately. */
	if(ctx && (ctx->query)) 
	{
		LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("asio_http_poll: try to send more data\n"));
		if(http_send_data(ctx)) 
		{
			/* If we wrote anything to be sent, go ahead and send it now. */
			LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\n"));
			tcp_output(pcb);
		}
	}
	return ERR_OK;
}

static err_t asio_http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct http_ctx* ctx = (struct http_ctx*) arg;

	os_printf("aio_http_recv: pcb=%p pbuf=%p err=%s len=%d tot_len=%d\n", 
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
			LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_recv: ctx is NULL, close\n"));
		}
		http_close_conn(pcb, ctx);
		return ERR_OK;
	}

	if(p->tot_len > RECV_BUF_SIZE)
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_recv: received data so big\n"));
		return ERR_MEM;
	}

	if (p->len != p->tot_len) 
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: incomplete header due to chained pbufs\n"));
	}

	if(ctx->query != NULL)
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_recv: received data while request perform\n"));
		return ERR_USE;
	}

	if((ctx->query = init_query()) == NULL)
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("asio_http_recv: init_query failed\n"));
		return ERR_MEM;
	}

	// копирование данных для парсинга
	memcpy(ctx->receive_buffer, p->payload, p->len);
	ctx->receive_buffer_size = p->len;
	ctx->pcb = pcb;

	err_t code = http_perform_request(ctx);
	if(code == ERR_OK)
	{
		tcp_recved(pcb, p->tot_len);
		pbuf_free(p);
	}
	return code;
}

static err_t asio_http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	/*struct http_state *hs;*/
	struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen*)arg;
	LWIP_UNUSED_ARG(err);
	LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept %p / %p\n", (void*)pcb, arg));

	/* Decrease the listen backlog counter */
	tcp_accepted(lpcb);
	/* Set priority */
	tcp_setprio(pcb, HTTPD_TCP_PRIO);

	/* Allocate memory for the structure that holds the state of the
	connection - initialized by that function. */
	struct http_ctx* ctx = init_ctx();
	if (ctx == NULL) 
	{
		LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept: Out of memory, RST\n"));
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


void asio_webserver_start(struct http_handler_rule *handlers)
{
	if(handlers == NULL)
	{
		handlers = user_handlers;
	}

	if(listen_pcb == NULL)
	{
		asio_init_ctx(IP_ADDR_ANY);
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
