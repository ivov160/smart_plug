#pragma once

#include <mesh.h>

#include <sys/socket.h>

#include <unistd.h>
#include <resolv.h>

#include <ev.h>

/**
 * @defgroup mesh_stub Mesh stub
 * @brief Реализация mesh для PC
 *
 * @addtogroup mesh_stub
 * @{
 */

/**
 * @brief Контект mesh для PC
 */
struct mesh_ctx 
{
	int port;									///< прорт для mesh
	int socket;									///< открытый сокет

	ev_periodic keep_alive_watcher;				///< handle на таймер libev
	ev_io socket_watcher;						///< handle наблюдателя за сокетом
	struct ev_loop* loop;						///< event_loop для работы libev

	struct mesh_message_handlers* handlers;		///< обработчики mesh сообщений
};

/**
 * @}
 */

