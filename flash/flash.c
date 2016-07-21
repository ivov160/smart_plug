#include "flash.h"
#include "esp_common.h"

#define FLASH_UNIT_SIZE 4
LOCAL uint32_t read_write_flash(uint32_t addr, uint8_t* data, uint32_t size, bool operation);

struct device_info_addr
{
	uint8_t device_type_offset;
	uint8_t device_id_offset;
	uint8_t active_offset;
	uint8_t reserved_offset;
};

struct wifi_info_addr
{
	uint32_t name_offset;
	uint32_t pass_offset;

	uint32_t ip_offset;
	uint32_t mask_offset;
	uint32_t gw_offset;
	uint32_t dns_offset;
};

static struct device_info_addr device_info_fields = 
{
	offsetof(struct device_info, device_type),
	offsetof(struct device_info, device_id),
	offsetof(struct device_info, active),
	offsetof(struct device_info, reserved),
};

static struct wifi_info_addr wifi_info_fields = 
{
	offsetof(struct wifi_info, name),
	offsetof(struct wifi_info, pass),
	offsetof(struct wifi_info, ip),
	offsetof(struct wifi_info, mask),
	offsetof(struct wifi_info, gw),
	offsetof(struct wifi_info, dns),
};

static struct layout_meta_info layout_info = 
{
	0, 
	FLASH_BASE_ADDR,

	CUSTOM_NAME_SIZE,
	FLASH_BASE_ADDR + CUSTOM_NAME_SIZE,

	CUSTOM_NAME_SIZE + sizeof(struct device_info),
	FLASH_BASE_ADDR + CUSTOM_NAME_SIZE + sizeof(struct device_info),

	CUSTOM_NAME_SIZE + sizeof(struct device_info) + sizeof(struct wifi_info) * WIFI_LIST_SIZE,
	FLASH_BASE_ADDR + CUSTOM_NAME_SIZE + sizeof(struct wifi_info) * WIFI_LIST_SIZE,

	/*FLASH_BASE_ADDR + CUSTOM_NAME_SIZE + sizeof(wifi_info) * DEVICE_LIST_SIZE ,*/
};

struct layout_meta_info* get_layout_info()
{
	return &layout_info;
}

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
		result = read_write_flash(layout_info.custom_name_addr, info->data, CUSTOM_NAME_SIZE, true) == SPI_FLASH_RESULT_OK;
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
		result = read_write_flash(layout_info.custom_name_addr, info->data, CUSTOM_NAME_SIZE, false) == SPI_FLASH_RESULT_OK;
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
		uint8_t* buffer = (uint8_t*)zalloc(sizeof(struct device_info));
		result = read_write_flash(layout_info.current_device_addr, buffer, sizeof(struct device_info), true) == SPI_FLASH_RESULT_OK;

		if(result)
		{
			info->device_type = buffer[device_info_fields.device_type_offset];
			info->device_id = buffer[device_info_fields.device_id_offset];
			info->active = buffer[device_info_fields.active_offset] > 0 ? 1 : 0;
			info->reserved = buffer[device_info_fields.reserved_offset];
		}
		free(buffer);
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
		uint8_t* buffer = (uint8_t*)zalloc(sizeof(struct device_info));

		buffer[device_info_fields.device_type_offset] = info->device_type;
		buffer[device_info_fields.device_type_offset] = info->device_type;
		buffer[device_info_fields.active_offset] = info->active;
		buffer[device_info_fields.reserved_offset] = info->reserved;

		result = read_write_flash(layout_info.current_device_addr, buffer, sizeof(struct device_info), false) == SPI_FLASH_RESULT_OK;
		free(buffer);
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
		uint8_t* buffer = (uint8_t*)zalloc(sizeof(struct wifi_info));
		uint32_t addr = layout_info.wifi_list_addr + sizeof(struct wifi_info) * index;
		result = read_write_flash(addr, buffer, sizeof(struct wifi_info), true) == SPI_FLASH_RESULT_OK;

		if(result)
		{
			memcpy(info->name, (buffer + wifi_info_fields.name_offset), WIFI_NAME_SIZE);
			memcpy(info->pass, (buffer + wifi_info_fields.pass_offset), WIFI_PASS_SIZE);

			//ставим явно в конце 0 дабы не подорваться
			info->name[WIFI_NAME_SIZE - 1] = 0;
			info->pass[WIFI_PASS_SIZE - 1] = 0;

			info->ip = (uint32_t) buffer[wifi_info_fields.ip_offset];
			info->mask = (uint32_t) buffer[wifi_info_fields.mask_offset];
			info->gw = (uint32_t) buffer[wifi_info_fields.gw_offset];
			info->dns = (uint32_t) buffer[wifi_info_fields.dns_offset];
		}
		free(buffer);
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
		uint8_t* buffer = (uint8_t*)zalloc(sizeof(struct wifi_info));

		memcpy((buffer + wifi_info_fields.name_offset), info->name, WIFI_NAME_SIZE);
		memcpy((buffer + wifi_info_fields.pass_offset), info->pass, WIFI_PASS_SIZE);

		// явно проставляем 0 в конце строк перед записью
		buffer[wifi_info_fields.name_offset + WIFI_NAME_SIZE - 1] = 0;
		buffer[wifi_info_fields.pass_offset + WIFI_PASS_SIZE - 1] = 0;

		memcpy((buffer + wifi_info_fields.ip_offset), (uint8_t*) &info->ip, sizeof(uint32_t));
		memcpy((buffer + wifi_info_fields.mask_offset), (uint8_t*) &info->mask, sizeof(uint32_t));
		memcpy((buffer + wifi_info_fields.gw_offset), (uint8_t*) &info->gw, sizeof(uint32_t));
		memcpy((buffer + wifi_info_fields.dns_offset), (uint8_t*) & info->dns, sizeof(uint32_t));

		uint32_t addr = layout_info.wifi_list_addr + sizeof(struct wifi_info) * index;
		result = read_write_flash(addr, buffer, sizeof(struct wifi_info), false) == SPI_FLASH_RESULT_OK;
		free(buffer);
	}
	else
	{
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, WIFI_LIST_SIZE);
	}
	return result;
}

/**
 * @brief Функция для чтения-записи данных
 * @param[in] addr Адресс на flash
 * @param[in] data Выделенный буфер (выделение происходит снаружи, также как и отчистка)
 * @param[in] size Размер буфера
 * @param[in] operation Требуемая опирация true - чтение с flash, false - запись на flash
 */
LOCAL uint32_t read_write_flash(uint32_t addr, uint8_t* data, uint32_t size, bool operation)
{
	uint32_t result = SPI_FLASH_RESULT_ERR;
	if(data != NULL)
	{
		uint32_t aligned_addr = addr & (-FLASH_UNIT_SIZE);
		uint32_t aligned_size = ((size + (FLASH_UNIT_SIZE - 1)) & -FLASH_UNIT_SIZE) + FLASH_UNIT_SIZE;

		result = operation 
			? spi_flash_read(aligned_addr, (uint32_t*)data, aligned_size)
			: spi_flash_write(aligned_addr, (uint32_t*)data, aligned_size);
	
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
