#ifndef __MESH_MESSAGE_H__
#define __MESH_MESSAGE_H__

#include "mesh_config.h"
#include "mesh_device_info.h"
#include "mesh_sender_info.h"


#if defined __cplusplus
extern "C" {
#endif

/**
 * @defgroup mesh Mesh 
 * @addtogroup mesh
 * @{
 */

/**
 * @see mesh.h
 */
struct mesh_ctx;

/**
 * @see mesh.h
 */
struct mesh_message_handlers;

/**
 * @brief Список комманд mesh протокола
 */
typedef enum 
{
	mesh_keep_alive = 0x0000001,						///< команда оповещения, что устройство в сети
	mesh_devices_info_request = 0x0000002,				///< команда опроса устройств сети
	mesh_device_info_response = 0x0000003,				///< команда ответа на опрос устройств сети
	mesh_device_info_response_confirm = 0x0000004		///< команда подтверждения получения ответа на опрос устройств сети
} mesh_message_command;

/**
 * @brief Сообщения передаваемые по сети
 * @note Планируемая последовательность передачи magic,command, sender_id,data_size,data
 * Но по факту может быть не такой, не проверялось
 */
struct mesh_message
{
	uint32_t magic;								///< магическая последовательность байт, для определения начала передачи (актуально при реализации обмена через uart или еще какое последовательное соеденение)
	mesh_message_command command;				///< mesh команда
	
	uint8_t data_size;							///< размер дополнительных данных
	uint8_t data[MESH_MESSAGE_DATA_SIZE];		///< дополнительные данные (интепритируются в зависимости от комманды)
};

/**
 * @brief Функция для созания сообщения
 * @param[in] command Команда сообщения
 * @param[in] data Передаваемые данные либо NULL
 * @param[in] size Размер передаваемых данных
 * @return Новое сообщение или NULL если не удалось создать
 */
struct mesh_message* new_message(mesh_message_command command, void* data, uint8_t size);

/**
 * @brief Функция для удаления сообщения
 *
 */
void free_message(struct mesh_message* msg);

/**
 * @brief Функция для вызова обработчика полученного сообщения
 * @param[in] handlers Список обработчиков сообщений
 * @param[in] mesh Контекст запущенного mesh
 * @param[in] sender Информация об отправители
 * @param[in] msg Полученное сообщение
 */
void call_handler(struct mesh_message_handlers* handlers, struct mesh_ctx* mesh, struct mesh_sender_info* sender, struct mesh_message* msg);

/**
 * @brief Функция для отпраки keep_alive сообщения
 * @param[in] mesh Контекст запущенного mesh (в данную сеть будет отправленно сообщение)
 * @param[in] info Информация об устройстве
 */
void mesh_send_keep_alive(struct mesh_ctx* mesh, struct mesh_device_info* info);

/**
 * @brief Функция для отправки широковещательного запроса об устройствах сети
 * @param[in] mesh Контекст запущенного mesh (в данную сеть будет отправленно сообщение)
 */
void mesh_send_request_devices_info(struct mesh_ctx* mesh); //oneshoot

/**
 * @brief Функция для отправки ответа на mesh_devices_info_request
 * @param[in] mesh Контекст запущенного mesh (в данную сеть будет отправленно сообщение)
 * @param[in] info Информация об устройстве
 * @param[in] dst Адресс назначения
 */
void mesh_send_device_info(struct mesh_ctx* mesh, struct mesh_device_info* info, uint32_t dst);

/**
 * @brief Функция для отправки подтвержения на получение mesh_device_info_response 
 * @param[in] mesh Контекст запущенного mesh (в данную сеть будет отправленно сообщение)
 * @param[in] dst Адресс назначения
 */
void mesh_send_request_device_info_confirm(struct mesh_ctx* mesh, uint32_t dst); 

/**
 * @}
 */

#if defined __cplusplus
}
#endif

#endif
