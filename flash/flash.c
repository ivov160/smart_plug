#include "flash.h"
#include "esp_common.h"

#define FLASH_UNIT_SIZE 4
#define ALIGNED_SIZE(size) ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE

LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation, bool xor);
/*LOCAL uint32_t erase_flash(uint32_t addr, uint32_t size);*/

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

	uint32_t wifi_list_count_offset;
	uint32_t wifi_list_offset;

	uint32_t device_list_count_offset;
	uint32_t device_list_offset;
};

static struct layout_meta_info layout_info = 
{
	0,								//custom_name offset

	CUSTOM_NAME_SIZE,				//device_info offset

	CUSTOM_NAME_SIZE + sizeof(struct device_info),							//wifi_list count offset
	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(uint32_t),		//wifi_list offset

	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(uint32_t) + sizeof(struct wifi_info) * WIFI_LIST_SIZE,						//device_info count offset
	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(uint32_t) + sizeof(struct wifi_info) * WIFI_LIST_SIZE + sizeof(uint32_t),						//device_info count offset

	/*FLASH_BASE_ADDR + CUSTOM_NAME_SIZE + sizeof(wifi_info) * DEVICE_LIST_SIZE ,*/
};

bool read_custom_name(struct custom_name* info)
{
	bool result = false;
	if(info != NULL)
	{
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.custom_name_offset, info->data, CUSTOM_NAME_SIZE, true, true) == SPI_FLASH_RESULT_OK;
	}
	else
	{
		os_printf("flash: custom_name is NULL\n");
	}
	return result;
}

bool write_custom_name(struct custom_name* info)
{
	bool result = false;
	if(info != NULL)
	{
		/*result = erase_flash(FLASH_BASE_ADDR + layout_info.custom_name_offset, sizeof(struct custom_name)) == SPI_FLASH_RESULT_OK;*/
		/*if(!result)*/
		/*{*/
			/*os_printf("spi: failed erase addr: 0x%x\n", FLASH_BASE_ADDR + layout_info.custom_name_offset);*/
		/*}*/

		/*if(result)*/
		/*{*/
			result = read_write_flash(FLASH_BASE_ADDR + layout_info.custom_name_offset, (void *) info, sizeof(struct custom_name), false, true) == SPI_FLASH_RESULT_OK;
		/*}*/
	}
	else
	{
		os_printf("flash: custom_name is NULL\n");
	}
	return result;
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
	bool result = false;
	if(info != NULL)
	{
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.current_device_offset, (void*) info, sizeof(struct device_info), true, true) == SPI_FLASH_RESULT_OK;
	}
	else
	{
		os_printf("flash: current device_info is NULL\n");
	}
	return result;
}

/*bool write_current_device(struct device_info* info)*/
/*{*/
	/*bool result = false;*/
	/*if(info != NULL)*/
	/*{*/
		/*result = read_write_flash(FLASH_BASE_ADDR + layout_info.current_device_offset, (void*) info, sizeof(struct device_info), false, true) == SPI_FLASH_RESULT_OK;*/
	/*}*/
	/*else*/
	/*{*/
		/*os_printf("flash: current device_info is NULL\n");*/
	/*}*/
	/*return result;*/
/*}*/

bool read_wifi_info(struct wifi_info* info, uint32_t index)
{
	bool result = false;
	if(info != NULL && index < WIFI_LIST_SIZE)
	{
		uint32_t count = get_wifi_info_list_size();
		if(count > index)
		{
			uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_offset + sizeof(struct wifi_info) * index;
			result = read_write_flash(addr, (void*) info, sizeof(struct wifi_info), true, true) == SPI_FLASH_RESULT_OK;
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
	return result;
}

bool write_wifi_info(struct wifi_info* info, uint32_t index)
{
	bool result = false;
	if(info != NULL && index < WIFI_LIST_SIZE)
	{
		uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_offset + sizeof(struct wifi_info) * index;
		result = read_write_flash(addr, (void*) info, sizeof(struct wifi_info), false, true) == SPI_FLASH_RESULT_OK;

		if(result)
		{
			uint32_t count = get_wifi_info_list_size();
			if(count == 0 || index > count - 1)
			{
				set_wifi_info_list_size(index + 1);
			}
		}
	}
	else
	{
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, WIFI_LIST_SIZE);
	}
	return result;
}

uint32_t get_wifi_info_list_size()
{
	uint32_t count = 0;
	uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_count_offset;
	uint32_t result = read_write_flash(addr, (void*) &count, sizeof(uint32_t), true, true);
	return count;
}

LOCAL void set_wifi_info_list_size(uint32_t count)
{
	uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_count_offset;
	uint32_t result = read_write_flash(addr, (void*) &count, sizeof(uint32_t), false, true);
	if(result != SPI_FLASH_RESULT_OK)
	{
		os_printf("spi: failed write data for addr: %x\n", addr);
	}
}

/**
 * @brief Функция для чтения-записи данных
 * @param[in] addr Адресс на flash
 * @param[in] data Выделенный буфер (выделение происходит снаружи, также как и отчистка)
 * @param[in] size Размер буфера
 * @param[in] operation Требуемая опирация true - чтение с flash, false - запись на flash
 */
LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation, bool xor)
{
	uint32_t result = SPI_FLASH_RESULT_ERR;
	if(data != NULL)
	{
		uint32_t aligned_addr = addr & (-FLASH_UNIT_SIZE);
		/*uint32_t aligned_size = ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE;*/

		// size allocated in bytes, but used pointer on 4 bytes 
		uint32_t* buffer = (uint32_t*)zalloc(size);
		if(operation)
		{
			result = spi_flash_read(aligned_addr, buffer, size);

			uint32_t* target = (uint32_t*) data;
			if(xor)
			{
				for(uint32_t i = 0; i < size / FLASH_UNIT_SIZE; ++i)
				{	
					target[i] = buffer[i] ^ 0XFFFFFFFF;
				}
			}
			else
			{
				memcpy(target, buffer, size);
			}
		}
		else
		{
			uint32_t* source = (uint32_t*) data;
			if(xor)
			{
				for(uint32_t i = 0; i < size / FLASH_UNIT_SIZE; ++i)
				{	
					buffer[i] = source[i] ^ 0XFFFFFFFF;
				}
			}
			else
			{
				memcpy(buffer, source, size);
			}

			result = spi_flash_write(aligned_addr, buffer, size);
		}
		free(buffer);
	
		if(result != SPI_FLASH_RESULT_OK)
		{
			os_printf("spi: failed %s data for addr: %x\n", (operation ? "read" : "write"), aligned_addr);
		}
	}
	else
	{
		os_printf("flash: read/write data buffer is NULL\n");
	}
	return result;
}

/*LOCAL uint32_t erase_flash(uint32_t addr, uint32_t size)*/
/*{*/
	/*uint32_t result = SPI_FLASH_RESULT_ERR;*/

	/*[>uint32_t buffer = 0XFFFFFFFF;<]*/
	/*[>uint32_t buffer = 0X00003333;<]*/
	/*[>uint32_t buffer = 0X33333333;<]*/
	/*[>uint32_t buffer = 0XCCCCCCCC;<]*/
	/*[>uint32_t buffer = 0X03030303;<]*/
	/*[>uint32_t buffer = 0X11111111;<]*/
	/*[>uint32_t buffer = 0X00000000;<]*/
	/*[>uint32_t* buffer = (uint32_t*)malloc(size);<]*/
	/*[>memset(buffer, 0XFFFFFFFF, size);<]*/
	/*[>memset(buffer, 0, size);<]*/
	/*result = spi_flash_erase_sector(110592);*/

	/*[>uint32_t buffer = 0XCCCCAAAA;<]*/
	/*[>uint32_t buffer = 0X33333333;<]*/
	/*[>result = spi_flash_write(addr, &buffer, sizeof(buffer));<]*/
	/*[>result = read_write_flash(addr, (void*) &buffer, sizeof(uint32_t), false, false);<]*/

	/*[>uint32_t* buffer = (uint32_t*)zalloc(size);<]*/
	/*[>if(result != SPI_FLASH_RESULT_OK)<]*/
	/*[>{<]*/
		/*[>os_printf("spi: failed read old value\n");<]*/
	/*[>}<]*/

	/*[>//проверка на FF<]*/
	/*[>if(buffer[0] != 0)<]*/
	/*[>{<]*/
		/*[>if(result == SPI_FLASH_RESULT_OK)<]*/
		/*[>{<]*/
			/*[>result = read_write_flash(addr, (void*) buffer, size, false, false);<]*/
			/*[>if(result != SPI_FLASH_RESULT_OK)<]*/
			/*[>{<]*/
				/*[>os_printf("spi: failed write xored value\n");<]*/
			/*[>}<]*/
		/*[>}<]*/
	/*[>}<]*/
	/*return result;*/
/*}*/
