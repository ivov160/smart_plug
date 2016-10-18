#ifndef __DEVICE_INFO_H__
#define __DEVICE_INFO_H__

#include <ctype.h>
#include <stdint.h>

#define DEVICE_NAME_SIZE 64

struct mesh_device_info
{
	uint8_t type;
	uint8_t id;
	uint32_t ip;
	char name[DEVICE_NAME_SIZE];
};

#endif
