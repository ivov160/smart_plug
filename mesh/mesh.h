#ifndef __MESH_H__
#define __MESH_H__

#include <ctype.h>
#include <stdint.h>

#include "mesh_config.h"
#include "mesh_message.h"
#include "mesh_sender_info.h"
#include "mesh_device_info.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @defgroup mesh Mesh 
 * @brief Библиотека MESH
 *
 * Данная библиотека предаставляет небольшое api 
 * для рассылки сообщений и их первичной обработки
 *
 * Для того, чтобы запустить mesh придется реализовать ряд функция
 * для используемой платформы. Примером может служить @mesh_stub или @user_mesh
 *
 * @addtogroup mesh
 * @{
 */

/**
 * @brief Контекст mesh сети
 * Данная структура не имеет реализации и реализуется в порте. 
 * В общем случае mesh_ctx содежит хендлер сокета, номер порта, список обработчиков сообщений
 */
struct mesh_ctx;

/**
 * @brief Сигнатура обработчика сообщений (struct mesh_message)
 */
typedef void (* mesh_message_handler)(struct mesh_ctx* ctx, struct mesh_sender_info* sender, struct mesh_message* message);

/**
 * @brief Структура задающая обработчик команды
 */
struct mesh_message_handlers
{
	mesh_message_command command;		///< обрабатываемая комманда
	mesh_message_handler handler;		///< обработчик сообщения
};


/**
 * @brief Сигнатура функции для запуска mesh
 * @note Это только сигнатура, сама реализация делается под каждый проект и платформу своя
 */
struct mesh_ctx* mesh_start(struct mesh_message_handlers* handlers, uint32_t addr, uint32_t port);

/**
 * @brief Сигнатура остановки mesh
 * @note Это только сигнатура, сама реализация делается под каждый проект и платформу своя
 */
void mesh_stop(struct mesh_ctx* ctx);

/**
 * @brief Сигнатура функции отправки данных в mesh
 * @note Это только сигнатура, сама реализация делается под каждый проект и платформу своя
 */
uint32_t mesh_send_data(struct mesh_ctx* ctx, void* data, uint32_t size, uint32_t ip);

/**
 * @brief Сигнатура функции получения данных из mesh
 * @note Это только сигнатура, сама реализация делается под каждый проект и платформу своя
 */
uint32_t mesh_receive_data(struct mesh_ctx* ctx, void* data, uint32_t size);

/**
 * @}
 */

#if defined __cplusplus
}
#endif

#endif
