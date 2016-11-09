#ifndef __ESP_MESH_H__
#define __ESP_MESH_H__

#include "esp_common.h"
#include "esp_libc.h"
#include "user_config.h"

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/udp.h"

#include "../mesh/mesh.h"

/**
 * @defgroup user User 
 * @defgroup user_mesh User mesh
 * @brief Пользовательский код для mesh
 *
 * @addtogroup user
 * @{
 * @addtogroup user_mesh
 * @{
 */

/** 
 * @brief Стуктура описывающая контекст mesg для esp
 */
struct mesh_ctx
{
	uint32_t port;									///< порт на котором слушаются пакеты
	struct udp_pcb* socket;							///< открытый сокет (используется LwIP RAW API)
	struct mesh_message_handlers* handlers;			///< список обработчиков команд
};

/**
 * @}
 * @}
 */

#endif
