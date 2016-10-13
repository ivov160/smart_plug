#ifndef __DEVICE_INFO_H__
#define __DEVICE_INFO_H__

#include <ctype.h>
#include <stdint.h>

struct device_info
{
	uint8_t device_type;
	uint8_t device_id;
	uint8_t active;
	uint8_t reserved;
};

#endif
