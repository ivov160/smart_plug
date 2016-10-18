#include "user_mesh.h"
#include "message.h"

#define RECV_BUF_SIZE 1024

static void asio_mesh_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
{
	if(arg != NULL)
	{
		struct mesh_ctx* ctx = (struct mesh_ctx*) arg;
		os_printf("mesh[asio_mesh_recv_callback]: pcb: %p, pbuf: %p\n", pcb, p);

		if(p != NULL)
		{
			if(p->tot_len > RECV_BUF_SIZE)
			{
				os_printf("mesh[asio_mesh_recv_callback]: received data too big, total: %d, max: %d\n", p->tot_len, RECV_BUF_SIZE);
			}
			else
			{
				if (p->len != p->tot_len) 
				{
					os_printf("mesh[asio_mesh_recv_callback]: incomplete header due to chained pbufs\n");
				}
				else if(p->len != sizeof(struct mesh_message))
				{
					os_printf("mesh[asio_mesh_recv_callback]: message size mismatch\n");
				}
				else
				{
					struct mesh_message* msg = (struct mesh_message*) p->payload;
					os_printf("mesh[asio_mesh_recv_callback]: message.magic: %d, message.command: %d, message.sender_id: %d, message.data_size: %d\n", 
							msg->magic, msg->command, msg->sender_id, msg->data_size);

					struct mesh_sender_info sender;
					sender.ip = addr->addr;
					sender.port = port;

					call_handler(ctx->handlers, ctx, &sender, msg);
				}
			}
			pbuf_free(p);
		}
	}
	else
	{
		LOG("mesh[asio_mesh_recv_callback]: invalid argument\n");
	}
}

static void asio_init_mesh_ctx(struct mesh_ctx* ctx, uint32_t addr, uint32_t port)
{
	LWIP_ASSERT("mesh[asio_init_mesh_ctx]: udp_new failed", ctx != NULL);

	ctx->socket = udp_new();
	ctx->port = port;

	ip_addr_t local_addr;
	local_addr.addr = addr;

	err_t err = udp_bind(ctx->socket, &local_addr, ctx->port);
	LWIP_ASSERT("mesh[asio_init_mesh_ctx]: udp_bind failed", err == ERR_OK);

	udp_recv(ctx->socket, asio_mesh_recv_callback, ctx);
}


struct mesh_ctx* mesh_start(struct mesh_message_handlers* handlers, uint32_t addr, uint32_t port)
{
	vPortEnterCritical();
	struct mesh_ctx* ctx = (struct mesh_ctx*) zalloc(sizeof(struct mesh_ctx));
	if(ctx != NULL)
	{
		ctx->handlers = handlers;
		asio_init_mesh_ctx(ctx, addr, port);
	}
	else
	{
		LOG("mesh[mesh_start]: failed create ctx\n");
	}
	vPortExitCritical();
}

void mesh_stop(struct mesh_ctx* ctx)
{
	vPortEnterCritical();
	if(ctx != NULL)
	{
		udp_disconnect(ctx->socket);
		udp_remove(ctx->socket);

		free(ctx);
		ctx = NULL;
	}
	vPortExitCritical();
}

uint32_t mesh_send_data(struct mesh_ctx* ctx, void* data, uint32_t size, uint32_t ip)
{
	struct pbuf* buffer = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
	if(buffer != NULL)
	{
		buffer->payload = data;

		ip_addr_t dst_addr;
		dst_addr.addr = ip;

		err_t result = udp_sendto(ctx->socket, buffer, &dst_addr, ctx->port);
		if(result != ERR_OK)
		{
			LOG("mesh[mesh_send_data]: failed send data, result: %d\n", result);
		}
	}
	else
	{
		LOG("mesh[mesh_send_data]: failed allocate response buffer\n");
	}
}

uint32_t mesh_receive_data(struct mesh_ctx* ctx, void* data, uint32_t size)
{
	LOG("mesh[mesh_receive_data]: not implemented\n");
}
