#include <iostream>

#include "mesh_platform.h"
#include <arpa/inet.h>

void mesh_keep_alive_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != nullptr)
	{
		if(msg->data_size == sizeof(struct mesh_device_info))
		{
			struct mesh_device_info* info = (struct mesh_device_info*) msg->data;

			in_addr addr;
			addr.s_addr = info->ip;
			printf("mesh[mesh_keep_alive_handler]: received info device_id: %d, device_type: %d, device_ip: %s, device_name: %s\n", 
					info->id, info->type, inet_ntoa(addr), info->name);
		}
		else
		{
			std::cout << "invalid message data size" << std::endl;
		}
	}
	else
	{
		std::cout << "message is nullptr" << std::endl;
	}
}

void mesh_devices_info_request_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != nullptr)
	{
		if(msg->data_size == sizeof(struct mesh_device_info))
		{
			mesh_device_info info;
			info.type = 3;
			info.id = 0;
			snprintf(info.name, DEVICE_NAME_SIZE, "PC-stub");
			info.ip = 0xC0A800; //192.168.0.110

			std::cout << "mesh[mesh_devices_info_request_handler]: send_device_info called" << std::endl;
			send_device_info(ctx, &info, sender->ip);
		}
		else
		{
			std::cout << "invalid message data size" << std::endl;
		}
	}
	else
	{
		std::cout << "message is nullptr" << std::endl;
	}
}

void mesh_device_info_response_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != nullptr)
	{
		if(msg->data_size == sizeof(struct mesh_device_info))
		{
			struct mesh_device_info* info = (struct mesh_device_info*) msg->data;

			in_addr addr;
			addr.s_addr = info->ip;

			in_addr sender_addr;
			sender_addr.s_addr = sender->ip;
			printf("mesh[mesh_device_info_response_handler]: received info device_id: %d, device_type: %d, device_ip: %s, device_name: %s, sender: %s\n", 
					info->id, info->type, inet_ntoa(addr), info->name, inet_ntoa(sender_addr));

			send_request_device_info_confirm(ctx, sender->ip);
		}
		else
		{
			std::cout << "invalid message data size" << std::endl;
		}
	}
	else
	{
		std::cout << "message is nullptr" << std::endl;
	}
}

void mesh_device_info_response_confirm_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != nullptr)
	{
		std::cout << "received device_info_response_confirm" << std::endl;
	}
	else
	{
		std::cout << "message is nullptr" << std::endl;
	}
}

static struct mesh_message_handlers mesh_handlers[] = 
{	
	{ mesh_keep_alive, mesh_keep_alive_handler },
	{ mesh_devices_info_request, mesh_devices_info_request_handler },
	{ mesh_device_info_response, mesh_device_info_response_handler },
	{ mesh_device_info_response_confirm, mesh_device_info_response_confirm_handler },
	{ mesh_keep_alive, NULL },
};


int main(int argc, const char** argv)
{   
	mesh_ctx* ctx = mesh_start(mesh_handlers, INADDR_ANY, 6636);
	if(ctx != nullptr)
	{
		mesh_stop(ctx);
	}
	else
	{
		std::cout << "failed start mesh" << std::endl;
	}
	
	return 0;
}

