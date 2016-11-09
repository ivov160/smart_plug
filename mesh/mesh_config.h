#ifndef __MESH_CONFIG_H__
#define __MESH_CONFIG_H__

#include "user_mesh_config.h"

/**
 * @defgroup mesh Mesh 
 * @addtogroup mesh
 * @{
 */

/**
 * @brief Мкрос для вывода логов
 */
#ifndef LOG
	#define LOG printf
#endif

/**
 * @brief Широковищательный ардесс
 * На самом деле зависит от платформы и библиотеки
 */
#ifndef BROADCAST_ADDR
	#define BROADCAST_ADDR 0xFFFFFFFF
#endif

/**
 * @brief Любой IP
 * На самом деле зависит от платформы и библиотеки
 */
#ifndef ANY_ADDR
	#define ANY_ADDR 0x00000000
#endif

/**
 * @brief Размер UDP пакета
 */
#ifndef MESH_RECV_BUF_SIZE 
	#define MESH_RECV_BUF_SIZE 1024
#endif

/**
 * @brief Размер поля name в struct mesh_device_info
 */
#ifndef MESH_DEVICE_NAME_SIZE 
	#define MESH_DEVICE_NAME_SIZE 64
#endif

/** 
 * @brief Размер данных передаваемых в mesh_message, struct mesh_message::data
 */
#ifndef MESH_MESSAGE_DATA_SIZE 
	#define MESH_MESSAGE_DATA_SIZE 512
#endif

/**
 * @}
 */

#endif
