#include "user_mesh.h"
#include "../mesh/mesh_message.h"

#include "../data/data.h"
#include "user_wifi.h"

/**
 * @defgroup user User 
 * @defgroup user_mesh User mesh
 *
 * @addtogroup user
 * @{
 * @addtogroup user_mesh
 * @{
 */

static os_timer_t mesh_keep_alive_timer;

static void mesh_keep_alive_timer_handler(void *p_args)
{
	if(p_args != NULL)
	{
		struct mesh_ctx* ctx = (struct mesh_ctx*) p_args;

		struct data_custom_name device_name;
		memset(&device_name, 0, sizeof(struct data_custom_name));

		struct data_device_info device_info;
		memset(&device_info, 0, sizeof(struct data_device_info));

		struct ip_info device_ip;
		memset(&device_ip, 0, sizeof(struct ip_info));

		if(data_read_custom_name(&device_name) && data_read_current_device(&device_info) && wifi_get_ip(&device_ip))
		{
			struct mesh_device_info mesh_device_info;
			memset(&mesh_device_info, 0, sizeof(struct mesh_device_info));

			mesh_device_info.id = device_info.device_id;
			mesh_device_info.type = device_info.device_type;
			mesh_device_info.ip = device_ip.ip.addr;
			memcpy(mesh_device_info.name, device_name.data, MESH_DEVICE_NAME_SIZE);

			mesh_send_keep_alive(ctx, &mesh_device_info);
		}
		else
		{
			os_printf("mesh[mesh_keep_alive_timer_handler]: failed fetch data\n");
		}
	}
	else
	{
		LOG("mehs[mesh_keep_alive_timer_handler]: args is null\n");
	}
}

static void asio_mesh_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
{
	if(arg != NULL)
	{
		struct mesh_ctx* ctx = (struct mesh_ctx*) arg;
		os_printf("mesh[asio_mesh_recv_callback]: pcb: %p, pbuf: %p\n", pcb, p);

		if(p != NULL)
		{
			if(p->tot_len > MESH_RECV_BUF_SIZE)
			{
				os_printf("mesh[asio_mesh_recv_callback]: received data too big, total: %d, max: %d\n", p->tot_len, MESH_RECV_BUF_SIZE);
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
					os_printf("mesh[asio_mesh_recv_callback]: message.magic: %d, message.command: %d, message.data_size: %d\n", 
							msg->magic, msg->command, msg->data_size);

					struct mesh_sender_info sender;
					/*sender.ip = addr->addr;*/
					/*sender.port = port;*/
					sender.ip = ntohl(addr->addr);
					sender.port = ntohs(port);


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

/**
 * @brief Инцилазация mesh
 */
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

		/*os_timer_setfn(&mesh_keep_alive_timer, mesh_keep_alive_timer_handler, ctx);*/
		/*os_timer_arm(&mesh_keep_alive_timer, 4000, true);*/
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
	os_timer_disarm(&mesh_keep_alive_timer);

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
	uint32_t sended = 0;
	struct pbuf* buffer = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
	if(buffer != NULL)
	{
		memcpy(buffer->payload, data, size);

		ip_addr_t dst_addr;
		dst_addr.addr = ip;

		err_t result = udp_sendto(ctx->socket, buffer, &dst_addr, ctx->port);
		if(result != ERR_OK)
		{
			LOG("mesh[mesh_send_data]: failed send data, result: %d\n", result);
		}
		else
		{
			sended = size;
		}
		pbuf_free(buffer);
	}
	else
	{
		LOG("mesh[mesh_send_data]: failed allocate response buffer\n");
	}
	return sended;
}

// для LwIP не нужно, по этому просто заглушк
uint32_t mesh_receive_data(struct mesh_ctx* ctx, void* data, uint32_t size)
{
	uint32_t received = 0;
	LOG("mesh[mesh_receive_data]: not implemented\n");
	return received;
}

/**
 * @}
 * @}
 */
