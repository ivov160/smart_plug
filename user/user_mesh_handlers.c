#include "user_mesh_handlers.h"
#include "../mesh/message.h"
#include "device_info.h"

void mesh_keep_alive_handler(struct mesh_message* msg)
{
	if(msg != NULL)
	{
		if(msg->data_size == sizeof(struct device_info))
		{
			struct device_info* info = (struct device_info*) msg->data;
			os_printf("mesh[mesh_keep_alive_handler]: received keep alive device_id: %d, device_type: %d\n", info->device_id, info->device_type);
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
