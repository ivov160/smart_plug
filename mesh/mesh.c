#include "mesh.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"

#include <string.h>
#include <stdlib.h>

#define MESH_PORT 6636
#define RECV_BUF_SIZE 1024

#define MESH_MESSAGE_DATA_SIZE 512

struct mesh_message
{
	uint32_t magic;
	uint32_t command;
	uint8_t sender;
	uint8_t data_size;
	uint8_t data[MESH_MESSAGE_DATA_SIZE];
};

static const uint32_t keep_alive = 0x00000010;
/*struct mesh_message_command*/
/*{*/
	/*static const uint32_t keep_alive = 0x00000010;*/
/*};*/


struct mesh_message* new_message(uint8_t command, void* data, uint8_t size)
{
	struct mesh_message* msg = (struct mesh_message*) malloc(sizeof(struct mesh_message));
	memset(msg, 0, sizeof(struct mesh_message));

	msg->magic = 0x00110110;
	msg->command = command;
	if(data != NULL)
	{
		memcpy(msg->data, data, size);
		msg->data_size = size;
	}

	return msg;
}

void free_message(struct mesh_message* msg)
{
	free(msg);
}


static struct udp_pcb* listen_pcb = NULL;

static void asio_mesh_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
{
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
				os_printf("mesh[asio_mesh_recv_callback]: message.magic: %d, message.command: %d, message.sender: %d, message.data_size: %d\n", msg->magic, msg->command, msg->sender, msg->data_size);
			}
		}
		pbuf_free(p);
	}
}

static void asio_init_mesh_ctx(ip_addr_t *local_addr)
{
	listen_pcb = udp_new();
	LWIP_ASSERT("asio_init_mesh_ctx: udp_new failed", listen_pcb != NULL);

	err_t err = udp_bind(listen_pcb, local_addr, MESH_PORT);
	LWIP_ASSERT("asio_init_mesh_ctx: udp_bind failed", err == ERR_OK);

	udp_recv(listen_pcb, asio_mesh_recv_callback, NULL);
}

void asio_mesh_start()
{
	vPortEnterCritical();
	if(listen_pcb == NULL)
	{
		asio_init_mesh_ctx(IP_ADDR_ANY);
	}
	vPortExitCritical();
}

int8_t asio_mesh_stop(void)
{
	vPortEnterCritical();
	if(listen_pcb != NULL)
	{
		udp_disconnect(listen_pcb);
		udp_remove(listen_pcb);
		listen_pcb = NULL;
	}
	vPortExitCritical();
}
