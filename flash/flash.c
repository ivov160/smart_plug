#include "flash.h"
#include "esp_common.h"

#define FLASH_UNIT_SIZE 4
#define ALIGNED_SIZE(size) ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE

LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation);
LOCAL void set_wifi_info_list_size(uint32_t count);

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

bool erase_layout()
{
	uint8_t buffer[FLASH_SIZE] = { 0 };
	return read_write_flash(FLASH_BASE_ADDR, buffer, FLASH_SIZE, false) == SPI_FLASH_RESULT_OK;
}

bool read_custom_name(struct custom_name* info)
{
	bool result = false;
	if(info != NULL)
	{
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.custom_name_offset, info->data, CUSTOM_NAME_SIZE, true) == SPI_FLASH_RESULT_OK;
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
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.custom_name_offset, (void*) info, sizeof(struct custom_name), false) == SPI_FLASH_RESULT_OK;
	}
	else
	{
		os_printf("flash: custom_name is NULL\n");
	}
	return result;
}

bool read_current_device(struct device_info* info)
{
	bool result = false;
	if(info != NULL)
	{
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.current_device_offset, (void*) info, sizeof(struct device_info), true) == SPI_FLASH_RESULT_OK;
	}
	else
	{
		os_printf("flash: current device_info is NULL\n");
	}
	return result;
}

bool write_current_device(struct device_info* info)
{
	bool result = false;
	if(info != NULL)
	{
		result = read_write_flash(FLASH_BASE_ADDR + layout_info.current_device_offset, (void*) info, sizeof(struct device_info), false) == SPI_FLASH_RESULT_OK;
	}
	else
	{
		os_printf("flash: current device_info is NULL\n");
	}
	return result;
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
			if(count == -1 || index > count)
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
LOCAL uint32_t read_write_flash(uint32_t addr, void* data, uint32_t size, bool operation)
{
	uint32_t result = SPI_FLASH_RESULT_ERR;
	if(data != NULL)
	{
		uint32_t aligned_addr = addr & (-FLASH_UNIT_SIZE);
		/*uint32_t aligned_size = ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE;*/

		if(operation)
		{
			result = spi_flash_read(aligned_addr, (uint32_t*) data, size);
		}
		else
		{
			result = spi_flash_write(aligned_addr, (uint32_t*) data, size);
		}
	
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
