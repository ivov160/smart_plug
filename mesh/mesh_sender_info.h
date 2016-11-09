#ifndef __MESH_SENDER_INFO_H__
#define __MESH_SENDER_INFO_H__

#include <ctype.h>
#include <stdint.h>

/**
 * @defgroup mesh Mesh 
 * @addtogroup mesh
 * @{
 */

/**
 * @brief Структура с информацией об отправители сообщения
 */
struct mesh_sender_info
{
	uint32_t ip;		///< ip источника пакета
	uint32_t port;		///< порт с которого был отправлен пакет
};

/**
 * @}
 */

#endif
