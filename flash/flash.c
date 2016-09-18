#include "flash.h"
#include "flash_hal.h"

// segment_data_size - crc meta info
#define SEGMENT_CRC_SIZE sizeof(uint32_t)
#define SEGMENT_DATA_SIZE FLASH_SEGMENT_SIZE - SEGMENT_CRC_SIZE

#define FLASH_UNIT_SIZE 4
#define ALIGNED_SIZE(size) ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE

LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation);
LOCAL void set_wifi_info_list_size(uint32_t count);

static uint8_t device_types_map_size = 6;
static char* device_types_map[] = 
{
	"UNKNOWN", "DUO", "DIMMER", "PLUG", "MOTION", "BULB"
};

struct layout_meta_info
{
	uint32_t custom_name_offset;

	uint32_t current_device_offset;

	uint32_t main_wifi_offset;

	uint32_t wifi_list_count_offset;
	uint32_t wifi_list_offset;

	uint32_t device_list_count_offset;
	uint32_t device_list_offset;
};

static struct layout_meta_info layout_info = 
{
	0,								//custom_name offset

	CUSTOM_NAME_SIZE,				//device_info offset

	CUSTOM_NAME_SIZE + sizeof(struct device_info),							//main wifi offset

	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(struct wifi_info),		//wifi_list count offset
	/*CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(wifi_info),		//wifi_list count offset*/
	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(struct wifi_info) + sizeof(uint32_t),		//wifi_list offset

	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(struct wifi_info) + sizeof(uint32_t) + sizeof(struct wifi_info) * WIFI_LIST_SIZE,						//device_info count offset
	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(struct wifi_info) + sizeof(uint32_t) + sizeof(struct wifi_info) * WIFI_LIST_SIZE + sizeof(uint32_t),						//device_info count offset

	/*FLASH_BASE_ADDR + CUSTOM_NAME_SIZE + sizeof(wifi_info) * DEVICE_LIST_SIZE ,*/
};

flash_t main_area = NULL;
flash_t shadow_area = NULL;

void init_layout()
{
	if(main_area == NULL)
	{
		main_area = flash_hal_init(0, layout_info.device_list_offset + sizeof(struct device_info) * DEVICE_LIST_SIZE);
		if(main_area == NULL)
		{
			os_printf("flash: failed init main_area\n");
		}
		else
		{
			os_printf("flash: main_area data_size: %d, real_size: %d\n", flash_hal_get_data_size(main_area), flash_hal_get_real_size(main_area));
		}
	}

	if(shadow_area == NULL)
	{
		shadow_area = flash_hal_init(flash_hal_get_real_size(main_area), layout_info.device_list_offset + sizeof(struct device_info) * DEVICE_LIST_SIZE);
		if(shadow_area == NULL)
		{
			os_printf("flash: failed init shadow area\n");
		}
		else
		{
			os_printf("flash: shadow_area data_size: %d, real_size: %d\n", flash_hal_get_data_size(shadow_area), flash_hal_get_real_size(shadow_area));
		}
	}
}

void destroy_layout()
{
	if(main_area != NULL)
	{
		flash_hal_destroy(main_area);
	}

	if(shadow_area != NULL)
	{
		flash_hal_destroy(shadow_area);
	}
}

bool read_custom_name(struct custom_name* info)
{
	flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = flash_hal_read(main_area, layout_info.custom_name_offset, (void*) info, CUSTOM_NAME_SIZE);
		if(result != FLASH_OK)
		{
			os_printf("flash: failed read custom_name, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: custom_name is NULL\n");
	}
	return result == FLASH_OK;
}

bool write_custom_name(struct custom_name* info)
{
	flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = flash_hal_write(main_area, layout_info.custom_name_offset, (void*) info, sizeof(struct custom_name));
		if(result != FLASH_OK)
		{
			os_printf("flash: failed write custom_name, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: custom_name is NULL\n");
	}
	return result == FLASH_OK;
}

bool device_info_get_powered(struct device_info* info)
{
	return info != NULL ? info->device_type >> 7 : 0;
}

uint8_t device_info_get_type_int(struct device_info* info)
{
	return info != NULL ? info->device_type & 0x0F : 0;
}

const char* device_info_get_type(struct device_info* info)
{
	uint8_t type = device_info_get_type_int(info);
	return type > device_types_map_size ? device_types_map[0] : device_types_map[type];
}

bool read_current_device(struct device_info* info)
{
	flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = flash_hal_read(main_area, layout_info.current_device_offset, (void*) info, sizeof(struct device_info));
		if(result != FLASH_OK)
		{
			os_printf("flash: failed read flash, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: current device_info is NULL\n");
	}
	return result == FLASH_OK;
}

bool read_wifi_info(struct wifi_info* info, uint32_t index)
{
	flash_code result = FLASH_OK;
	if(info != NULL && index < WIFI_LIST_SIZE)
	{
		uint32_t count = get_wifi_info_list_size();
		if(count > index)
		{
			flash_code result = flash_hal_read(main_area, layout_info.wifi_list_offset + sizeof(struct wifi_info) * index, (void*) info, sizeof(struct wifi_info));;
			if(result != FLASH_OK)
			{
				os_printf("flash: failed read wifi info, result: %d\n", result);
			}
		}
		else
		{
			os_printf("flash: out of list range\n");
		}
	}
	else
	{
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, WIFI_LIST_SIZE);
	}
	return result == FLASH_OK;
}

bool write_wifi_info(struct wifi_info* info, uint32_t index)
{
	flash_code result = FLASH_OK;
	if(info != NULL && index < WIFI_LIST_SIZE)
	{
		result = flash_hal_write(main_area, layout_info.wifi_list_offset + sizeof(struct wifi_info) * index, (void*) info, sizeof(struct wifi_info));
		if(result == FLASH_OK)
		{
			uint32_t count = get_wifi_info_list_size();
			if(count == 0 || index > count - 1)
			{
				set_wifi_info_list_size(index + 1);
			}
		}
		else
		{
			os_printf("flash: failed write wifi_info, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, WIFI_LIST_SIZE);
	}
	return result == FLASH_OK;
}

bool read_main_wifi(struct wifi_info* info)
{
	flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = flash_hal_read(main_area, layout_info.main_wifi_offset, (void*) info, sizeof(struct wifi_info));
	}
	else
	{
		os_printf("flash: wifi_info is NULL\n");
	}
	return result == FLASH_OK;
}

bool write_main_wifi(struct wifi_info* info)
{
	flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = flash_hal_write(main_area, layout_info.main_wifi_offset, (void*) info, sizeof(struct wifi_info));
	}
	else
	{
		os_printf("flash: wifi_info is NULL\n");
	}
	return result == FLASH_OK;
}

bool erase_main_wifi()
{
	return flash_hal_erase(main_area, layout_info.main_wifi_offset, sizeof(struct wifi_info)) == FLASH_OK;
}

uint32_t get_wifi_info_list_size()
{
	uint32_t count = 0;
	flash_code result = flash_hal_read(main_area, layout_info.wifi_list_count_offset, (void*) &count, sizeof(uint32_t));;
	if(result != FLASH_OK)
	{
		os_printf("flash: failed read wifi info list size, result: %d\n", result);
		count = 0;
	}
	return count;
}

LOCAL void set_wifi_info_list_size(uint32_t count)
{
	flash_code result = flash_hal_write(main_area, layout_info.wifi_list_count_offset, (void*) &count, sizeof(uint32_t));;
	if(result != FLASH_OK)
	{
		os_printf("flash: failed write wifi info list size, result: %d\n", result);
	}
}


