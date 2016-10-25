#pragma once

#include "../../mesh/mesh.h"

#include <sys/socket.h>

#include <unistd.h>
#include <resolv.h>

#include <ev.h>

struct mesh_ctx 
{
	int port;
	int socket;

	ev_periodic keep_alive_watcher;
	ev_io socket_watcher;
	struct ev_loop* loop;

	struct mesh_message_handlers* handlers;
};

