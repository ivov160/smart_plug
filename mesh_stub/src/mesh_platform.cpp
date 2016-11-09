#include "mesh_platform.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#include <error.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <resolv.h>


/**
 * @defgroup mesh_stub Mesh stub
 * @addtogroup mesh_stub
 * @{
 */

static bool keep_alive = true;

static void emit_stub_message(struct ev_loop *loop, ev_periodic *w, int revents)
{
	if(!(EV_ERROR & revents))
	{

		void* ptr = ev_userdata(loop);
		if(ptr != 0)
		{
			mesh_ctx* ctx = reinterpret_cast<mesh_ctx*>(ptr);

			if(keep_alive)
			{
				mesh_device_info info;
				info.type = 3;
				info.id = 0;
				snprintf(info.name, MESH_DEVICE_NAME_SIZE, "PC-stub");
				info.ip = 0xC0A800; //192.168.0.110

				LOG("keep_alive message send\n");
				mesh_send_keep_alive(ctx, &info);
			}
			else
			{
				LOG("request_devices message send\n");
				mesh_send_request_devices_info(ctx);
			}
			keep_alive = !keep_alive;
		}
		else
		{
			LOG("mesh_ctx not setted\n");
		}
	}
	else
	{
		LOG("got invalid event: %i\n", revents);
	}
}


static void mesh_recv_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	if(!(EV_ERROR & revents))
	{
		void* ptr = ev_userdata(loop);
		if(ptr != 0)
		{
			mesh_ctx* ctx = reinterpret_cast<mesh_ctx*>(ptr);

			uint8_t buffer[sizeof(mesh_message)] = { 0 };

			struct sockaddr_in srcaddr;
			socklen_t struct_size = sizeof(struct sockaddr_in);

			ssize_t readed_size = recvfrom(ctx->socket, buffer, sizeof(mesh_message), 0, reinterpret_cast<struct sockaddr*>(&srcaddr), &struct_size);
			if(readed_size == sizeof(struct mesh_message))
			{
				struct mesh_message* msg = reinterpret_cast<struct mesh_message*>(buffer);

				struct mesh_sender_info sender;
				sender.ip = ntohl(srcaddr.sin_addr.s_addr);
				sender.port = ntohs(srcaddr.sin_port);

				if(sender.ip != inet_addr("192.168.0.100"))
				{
					LOG("received command: %d\n", msg->command);
					call_handler(ctx->handlers, ctx, &sender, msg);
				}
			}
			else
			{
				LOG("failed read message, wrong size\n");
			}
		}
		else
		{
			LOG("mesh_ctx not setted\n");
		}
	}
	else
	{
		LOG("got invalid event: %i\n", revents);
	}
}

struct mesh_ctx* mesh_start(struct mesh_message_handlers* handlers, uint32_t ip, uint32_t port)
{
	mesh_ctx* ctx = new mesh_ctx;

	ctx->port = port;
	ctx->socket = socket(PF_INET, SOCK_DGRAM, 0);
	ctx->handlers = handlers;
	if(ctx->socket <= 0)
	{
		LOG("failed create socket, err: %s\n", strerror(errno));
		return nullptr;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(ctx->port);
	addr.sin_addr.s_addr = htonl(ip);

	if(bind(ctx->socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		LOG("failed bind socket, err: %s\n", strerror(errno));
		return nullptr;
	}

	int option_value = 1;
	if (setsockopt(ctx->socket, SOL_SOCKET, SO_BROADCAST, &option_value, sizeof(option_value)) == -1) 
	{
		LOG("failed  setsockopt (SO_BROADCAST), err: %s\n", strerror(errno));
		return nullptr;
	}

	ctx->loop = ev_loop_new(0);
	ev_set_userdata(ctx->loop, reinterpret_cast<void*>(ctx));

	ev_periodic_init(&ctx->keep_alive_watcher, emit_stub_message, 0., 10., 0);
	ev_periodic_start(ctx->loop, &ctx->keep_alive_watcher);

	ev_io_init(&ctx->socket_watcher, mesh_recv_cb, ctx->socket, EV_READ);
	ev_io_start(ctx->loop, &ctx->socket_watcher);

	ev_loop(ctx->loop, 0);

	return ctx;
}

void mesh_stop(struct mesh_ctx* ctx)
{
	if(ctx != nullptr)
	{
		ev_io_stop(ctx->loop, &ctx->socket_watcher);
		//ev_periodic_stop(ctx->loop, &ctx->keep_alive_watcher);

		close(ctx->socket);
		ev_loop_destroy(ctx->loop);
		delete ctx;
	}
}

uint32_t mesh_send_data(struct mesh_ctx* ctx, void* data, uint32_t size, uint32_t ip)
{
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(ctx->port);
	s.sin_addr.s_addr = htonl(ip);

	ssize_t sended_data = sendto(ctx->socket, data, size, 0, (struct sockaddr*) &s, sizeof(struct sockaddr_in));
	if(sended_data < 0)
	{
		LOG("failed send data, err: %s\n", strerror(errno));
	}
	return sended_data;
}

uint32_t mesh_receive_data(struct mesh_ctx* ctx, void* data, uint32_t size)
{
	struct sockaddr_in srcaddr;
	socklen_t struct_size = sizeof(struct sockaddr_in);

	ssize_t readed_size = recvfrom(ctx->socket, data, size, 0, reinterpret_cast<struct sockaddr*>(&srcaddr), &struct_size);
	if(readed_size < 0)
	{
		LOG("failed create socket, err: %s\n", strerror(errno));
	}
	return readed_size;
}

/**
 * @}
 */

