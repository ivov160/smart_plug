#ifndef __USER_MESH_HANDLERS_H__
#define __USER_MESH_HANDLERS_H__

#include "esp_common.h"
#include "mesh.h"

void mesh_keep_alive_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* message);
void mesh_devices_info_request_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* message);
void mesh_device_info_response_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* message);
void mesh_device_info_response_confirm_handler(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* message);

#endif
