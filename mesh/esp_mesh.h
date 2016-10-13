#ifndef __ESP_MESH_H__
#define __ESP_MESH_H__

#include "esp_common.h"
#include "esp_libc.h"
#include "user_config.h"

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"

#include "mesh.h"

struct mesh_ctx
{
	uint32_t port;
	struct udp_pcb* socket;
	struct mesh_message_handlers* handlers;
};

#endif
