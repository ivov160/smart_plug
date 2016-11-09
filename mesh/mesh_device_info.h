#ifndef __MESH_DEVICE_INFO_H__
#define __MESH_DEVICE_INFO_H__

#include <ctype.h>
#include <stdint.h>

#include "mesh_config.h"

/**
 * @defgroup mesh Mesh 
 * @addtogroup mesh
 * @{
 */

/**
 * @brief Структура инсормации об устройстве в сети
 * Данные структуры пересылаются в поле data некоторых сообщений: mesh_keep_alive, mesh_device_info_response 
 */
struct mesh_device_info
{
	uint8_t type;						///< тип устройства (разеточное, автономное)
	uint8_t id;							///< id устройства
	uint32_t ip;						///< ip устройства
	char name[MESH_DEVICE_NAME_SIZE];		///< имя устройства
};

/**
 * @}
 */

#endif
