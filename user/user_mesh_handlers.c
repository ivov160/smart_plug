#include "user_mesh_handlers.h"
#include "../mesh/message.h"
#include "device_info.h"

#include "../flash/flash.h"
#include "wifi_station.h"

void mesh_keep_alive_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != NULL)
	{
		if(msg->data_size == sizeof(struct mesh_device_info))
		{
			struct mesh_device_info* info = (struct mesh_device_info*) msg->data;
			os_printf("mesh[mesh_keep_alive_handler]: received info device_id: %d, device_type: %d, device_ip: %i, device_name: %s\n", 
					info->id, info->type, info->ip, info->name);
		}
		else
		{
			os_printf("mesh[mesh_keep_alive_handler]: invalid data size in mesh_keep_alive message\n");
		}
	}
	else
	{
		os_printf("mesh[mesh_keep_alive_handler]: mesh_message is null\n");
	}
}

void mesh_devices_info_request_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != NULL)
	{
		struct custom_name device_name;
		memset(&device_name, 0, sizeof(struct custom_name));

		struct device_info device_info;
		memset(&device_info, 0, sizeof(struct device_info));

		struct ip_info device_ip;
		memset(&device_ip, 0, sizeof(struct ip_info));

		if(read_custom_name(&device_name) && read_current_device(&device_info) && get_wifi_ip_info(&device_ip))
		{
			struct mesh_device_info mesh_device_info;
			memset(&mesh_device_info, 0, sizeof(struct mesh_device_info));

			mesh_device_info.id = device_info.device_id;
			mesh_device_info.type = device_info.device_type;
			mesh_device_info.ip = device_ip.ip.addr;
			memcpy(mesh_device_info.name, device_name.data, DEVICE_NAME_SIZE);

			send_device_info(ctx, &mesh_device_info, sender->ip);
		}
		else
		{
			os_printf("mesh[mesh_devices_info_request_handler]: failed read flash\n");
		}
	}
	else
	{
		os_printf("mesh[mesh_devices_info_request_handler]: mesh_message is null\n");
	}
}

void mesh_device_info_response_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != NULL)
	{
		if(msg->data_size == sizeof(struct mesh_device_info))
		{
			struct mesh_device_info* info = (struct mesh_device_info*) msg->data;
			os_printf("mesh[mesh_device_info_response_handler]: received info device_id: %d, device_type: %d, device_ip: %i, device_name: %s\n", 
					info->id, info->type, info->ip, info->name);

			send_request_device_info_confirm(ctx, sender->ip);
		}
		else
		{
			os_printf("mesh[mesh_device_info_response_handler]: invalid data size\n");
		}
	}
	else
	{
		os_printf("mesh[mesh_device_info_response_handler]: mesh_message is null\n");
	}
}

void mesh_device_info_response_confirm_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* msg)
{
	if(msg != NULL)
	{
		os_printf("mesh[mesh_device_info_response_confirm_handler]: received device_info_response_confirm\n");
	}
	else
	{
		os_printf("mesh[mesh_device_info_response_confirm_handler]: mesh_message is null\n");
	}
}
