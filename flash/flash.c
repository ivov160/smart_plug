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
	}

	if(shadow_area == NULL)
	{
		shadow_area = flash_hal_init(flash_hal_get_real_size(main_area), layout_info.device_list_offset + sizeof(struct device_info) * DEVICE_LIST_SIZE);
		if(shadow_area == NULL)
		{
			os_printf("flash: failed init shadow area\n");
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
	bool result = false;
	if(info != NULL && index < WIFI_LIST_SIZE)
	{
		uint32_t count = get_wifi_info_list_size();
		if(count > index)
		{
			uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_offset + sizeof(struct wifi_info) * index;
			result = read_write_flash(addr, (void*) info, sizeof(struct wifi_info), true) == SPI_FLASH_RESULT_OK;
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
		result = read_write_flash(addr, (void*) info, sizeof(struct wifi_info), false) == SPI_FLASH_RESULT_OK;

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
	uint32_t result = read_write_flash(addr, (void*) &count, sizeof(uint32_t), true);
	return count;
}

LOCAL void set_wifi_info_list_size(uint32_t count)
{
	uint32_t addr = FLASH_BASE_ADDR + layout_info.wifi_list_count_offset;
	uint32_t result = read_write_flash(addr, (void*) &count, sizeof(uint32_t), false);
	if(result != SPI_FLASH_RESULT_OK)
	{
		os_printf("flash: failed write data for addr: %x\n", addr);
	}
}

LOCAL bool check_crc(uint32_t *data, uint32_t size)
{
	return true;
}

LOCAL uint32_t read_sector(uint32_t *data, uint32_t i, bool shadow)
{
}

LOCAL uint32_t write_sector(uint32_t *data, uint32_t i, bool shadow)
{
}

LOCAL uint32_t erase_sector(uint32_t *data, uint32_t i, bool shadow)
{
}

LOCAL uint32_t read_range_sectors(uint32_t *data, uint32_t i, uint32_t j, bool shadow)
{
}

LOCAL uint32_t write_range_sectors(uint32_t *data, uint32_t i, uint32_t j, bool shadow)
{
}

LOCAL uint32_t erase_range_sectors(uint32_t *data, uint32_t i, uint32_t j, bool shadow)
{
}

/**
 * @brief Функция для чтения-записи данных
 * @param[in] addr Адресс на flash
 * @param[in] data Выделенный буфер (выделение происходит снаружи, также как и отчистка)
 * @param[in] size Размер буфера
 * @param[in] operation Требуемая опирация true - чтение с flash, false - запись на flash
 */
LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation)
{
	uint32_t result = SPI_FLASH_RESULT_ERR;
	if(data != NULL)
	{	
		/*// вычисление начала сигмента*/
		/*uint32_t flash_offset = addr - FLASH_BASE_ADDR;*/
		/*uint32_t start_index = flash_offset / FLASH_SEGMENT_SIZE;*/
		/*uint32_t end_index = (flash_offset + size) / FLASH_SEGMENT_SIZE;*/

		/*// включительно end_segment (+1)*/
		/*// возможно надо учитывать crc32*/
		/*uint32_t total_size = (end_index - start_index + 1) * FLASH_SEGMENT_SIZE;*/

		/*if(total_size < FLASH_BUFFER_SIZE)*/
		/*{*/
			/*uint32_t* buffer = (uint32_t*)malloc(total_size);*/
			/*// читаем сектора*/
			/*result = read_range_sectors(buffer, start_index, end_index + 1, false);*/
			/*if(result == SPI_FLASH_RESULT_OK)*/
			/*{*/
				
			/*}*/
			/*else*/
			/*{*/
			/*}*/


			/*uint32_t aligned_addr = (FLASH_BASE_ADDR + (start_index * FLASH_SEGMENT_SIZE)) & (-FLASH_UNIT_SIZE);*/

			/*result = spi_flash_read(aligned_addr, buffer, total_size);*/
			/*for(uint32_t i = start_index; i <= end_index; ++i)*/
			/*{*/
				/*if(!check_crc((buffer + i * FLASH_SEGMENT_SIZE), FLASH_SEGMENT_SIZE))*/
				/*{ // shadow read*/
				/*}*/
			/*}*/

			/*uint32_t* pointer = (uint32_t*) data;*/
			/*for(uint32_t i = 0; i < size / FLASH_UNIT_SIZE; ++i)*/
			/*{	*/
				/*pointer[i] = pointer[i] ^ 0XFFFFFFFF;*/
			/*}*/

			/*if(!operation)*/
			/*{*/
				/*uint32_t* source = (uint32_t*) data;*/
				/*if(xor)*/
				/*{*/
					/*for(uint32_t i = 0; i < size / FLASH_UNIT_SIZE; ++i)*/
					/*{	*/
						/*buffer[i] = source[i] ^ 0XFFFFFFFF;*/
					/*}*/
				/*}*/
				/*else*/
				/*{*/
					/*memcpy(buffer, source, size);*/
				/*}*/

				/*result = spi_flash_write(aligned_addr, buffer, size);*/
			/*}*/
			/*free(buffer);*/
		
			/*if(result != SPI_FLASH_RESULT_OK)*/
			/*{*/
				/*os_printf("flash: failed %s data for addr: %x\n", (operation ? "read" : "write"), aligned_addr);*/
			/*}*/
		/*}*/
		/*else*/
		/*{*/
			/*os_printf("flash: data total_size: %d overflow buffer size: %d\n", total_size, FLASH_BUFFER_SIZE);*/
		/*}*/
	}
	else
	{
		os_printf("flash: read/write data buffer is NULL\n");
	}
	return result;
}

