#ifndef __MESH_MESSAGE_H__
#define __MESH_MESSAGE_H__

#include "device_info.h"
#include "sender_info.h"

#define MESH_MESSAGE_DATA_SIZE 512

#if defined __cplusplus
extern "C" {
#endif

struct mesh_ctx;
struct mesh_message_handlers;

typedef enum 
{
	mesh_keep_alive = 0x0000001,
	mesh_devices_info_request = 0x0000002,
	mesh_device_info_response = 0x0000003,
	mesh_device_info_response_confirm = 0x0000004
} mesh_message_command;

struct mesh_message
{
	uint32_t magic;
	mesh_message_command command;

	uint8_t sender_id;

	uint8_t data_size;
	uint8_t data[MESH_MESSAGE_DATA_SIZE];
};


struct mesh_message* new_message(mesh_message_command command, void* data, uint8_t size);
void free_message(struct mesh_message* msg);

void call_handler(struct mesh_message_handlers* handlers, struct mesh_ctx* mesh, struct mesh_sender_info* sender, struct mesh_message* msg);

void send_keep_alive(struct mesh_ctx* mesh, struct mesh_device_info* info); //timer
void send_request_devices_info(struct mesh_ctx* mesh); //oneshoot
void send_device_info(struct mesh_ctx* mesh, struct mesh_device_info* info, uint32_t dst); //callback
void send_request_device_info_confirm(struct mesh_ctx* mesh, uint32_t dst); //callback

#if defined __cplusplus
}
#endif

#endif
