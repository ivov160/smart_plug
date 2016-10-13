#ifndef __USER_MESH_H__
#define __USER_MESH_H__

#include <ctype.h>
#include <stdint.h>

#include "message.h"

#ifndef LOG
	#define LOG printf
#endif

#ifndef BROADCAST_ADDR
	#define BROADCAST_ADDR 0xFFFFFFFF
#endif

struct mesh_ctx;

typedef void (* mesh_message_handler)(struct mesh_message* message);

struct mesh_message_handlers
{
	mesh_message_command command;
	mesh_message_handler handler;	
};

struct mesh_ctx* mesh_start(struct mesh_message_handlers* handlers, uint32_t addr, uint32_t port);
void mesh_stop(struct mesh_ctx* ctx);

uint32_t mesh_send_data(struct mesh_ctx* ctx, void* data, uint32_t size, uint32_t ip);
uint32_t mesh_receive_data(struct mesh_ctx* ctx, void* data, uint32_t size);

#endif
