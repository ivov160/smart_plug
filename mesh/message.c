#include "message.h"
#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


struct mesh_message* new_message(mesh_message_command command, void* data, uint8_t size)
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

void send_keep_alive(struct mesh_ctx* mesh, struct mesh_device_info* info)
{
	LOG("keep_alive broadcast\n");

	struct mesh_message* msg = new_message(mesh_keep_alive, info, sizeof(struct mesh_device_info));
	ssize_t msg_size = sizeof(struct mesh_message);

	uint32_t sended_data = mesh_send_data(mesh, (void*) msg, msg_size, BROADCAST_ADDR);
	if(sended_data < 0)
	{
		LOG("failed send data\n");
	}
	else if(msg_size != sended_data)
	{
		LOG("sending less data msg_size: %u, sended: %u\n", msg_size, sended_data);
	}
	free_message(msg);
}

void send_request_devices_info(struct mesh_ctx* mesh)
{
	LOG("send_request_devices_info\n");

	struct mesh_message* msg = new_message(mesh_devices_info_request , NULL, 0);
	ssize_t msg_size = sizeof(struct mesh_message);

	uint32_t sended_data = mesh_send_data(mesh, (void*) msg, msg_size, BROADCAST_ADDR);
	if(sended_data < 0)
	{
		LOG("failed send data\n");
	}
	else if(msg_size != sended_data)
	{
		LOG("sending less data msg_size: %u, sended: %u\n", msg_size, sended_data);
	}
	free_message(msg);
}

void send_device_info(struct mesh_ctx* mesh, struct mesh_device_info* info, uint32_t dst)
{
	LOG("send_device_info\n");

	struct mesh_message* msg = new_message(mesh_device_info_response, info, sizeof(struct mesh_device_info));
	ssize_t msg_size = sizeof(struct mesh_message);

	uint32_t sended_data = mesh_send_data(mesh, (void*) msg, msg_size, dst);
	if(sended_data < 0)
	{
		LOG("failed send data\n");
	}
	else if(msg_size != sended_data)
	{
		LOG("sending less data msg_size: %u, sended: %u\n", msg_size, sended_data);
	}
	free_message(msg);
}

void send_request_device_info_confirm(struct mesh_ctx* mesh, uint32_t dst)
{
	LOG("send_request_device_info_confirm\n");

	struct mesh_message* msg = new_message(mesh_device_info_response_confirm , NULL, 0);
	ssize_t msg_size = sizeof(struct mesh_message);

	uint32_t sended_data = mesh_send_data(mesh, (void*) msg, msg_size, dst);
	if(sended_data < 0)
	{
		LOG("failed send data\n");
	}
	else if(msg_size != sended_data)
	{
		LOG("sending less data msg_size: %u, sended: %u\n", msg_size, sended_data);
	}
	free_message(msg);
}


void call_handler(struct mesh_message_handlers* handlers, struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(handlers != NULL && msg != NULL)
	{
		struct mesh_message_handlers* handler = handlers;

		while(handler != NULL && handler->handler != NULL)
		{
			if(handler->command == msg->command)
			{
				break;
			}
			++handler;
		}

		// checking end of handlers list
		if(handler->handler != NULL)
		{
			handler->handler(ctx, sender, msg);
		}
		else
		{
			LOG("mesh[call_handler]: not found handler for command: %d\n", msg->command);
		}
	}
	else if(handlers == NULL)
	{
		LOG("mesh[call_handler]: handlers is null\n");
	}
	else if(msg == NULL)
	{
		LOG("mesh[call_handler]: msg is null\n");
	}
}
