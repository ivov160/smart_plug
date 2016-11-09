#include "data.h"
#include "flash.h"

/**
 * @defgroup flash Flash 
 * @brief Работа с данными на flash
 *
 * @defgroup data_api Data API
 * @brief API для работы с данными
 */

/** 
 * @addtogroup flash
 * @{
 *
 * @addtogroup data_api
 * @{
 */

/**
 * @brief Функция сохраняет на flash текущий размер массива data_wifi_info
 */
static data_flash_code data_set_wifi_info_list_size(uint32_t count);

// размер массива с строковыми именами типов устройств
static uint8_t device_types_map_size = 6;
// размер со строковыми именами типов устройств
static char* device_types_map[] = 
{
	"UNKNOWN", "DUO", "DIMMER", "PLUG", "MOTION", "BULB"
};

/**
 * @brief Структура задает схему хранения данных на flash
 * @note Все смещения указываются относительно начала area
 */
struct layout_meta_info
{
	uint32_t custom_name_offset;				///< смещение custom_name

	uint32_t current_device_offset;				///< смещение data_device_info текущего устройства

	uint32_t main_wifi_offset;					///< смещение основной wifi сети

	uint32_t wifi_list_count_offset;			///< смещение текущего размера массива data_wifi_info
	uint32_t wifi_list_offset;					///< смещение списка data_wifi_info

	uint32_t device_list_count_offset;			///< смещение текущего размера массива data_device_info
	uint32_t device_list_offset;				///< смещение списка data_device_info
};

// инициализация структуры реальной схемой
static struct layout_meta_info layout_info = 
{
	0,									//custom_name offset

	DATA_CUSTOM_NAME_SIZE,				//device_info offset

	DATA_CUSTOM_NAME_SIZE + sizeof(struct data_device_info),							//main wifi offset

	DATA_CUSTOM_NAME_SIZE + sizeof(struct data_device_info) + sizeof(struct data_wifi_info),		//wifi_list count offset
	DATA_CUSTOM_NAME_SIZE + sizeof(struct data_device_info) + sizeof(struct data_wifi_info) + sizeof(uint32_t),		//wifi_list offset

	DATA_CUSTOM_NAME_SIZE + sizeof(struct data_device_info) + sizeof(struct data_wifi_info) + sizeof(uint32_t) + sizeof(struct data_wifi_info) * DATA_WIFI_LIST_SIZE,						//device_info count offset
	DATA_CUSTOM_NAME_SIZE + sizeof(struct data_device_info) + sizeof(struct data_wifi_info) + sizeof(uint32_t) + sizeof(struct data_wifi_info) * DATA_WIFI_LIST_SIZE + sizeof(uint32_t),						//device_info count offset
};

//handle на основную область flash
data_flash_t main_area = NULL;
//handle на теневую область flash
data_flash_t shadow_area = NULL;

/**
 * @brief Функция проверки/востановления целосности данных
 * Если обе области целы, функция ниделает ничего.
 * Если одна из областей повреждена (crc не совпал), то поврежденная область
 * востанавливается из целой.
 * В противном случае возвращается FLASH_CHECKSUM_MISMATCH
 * @param[in] main Основная область flash
 * @param[in] shadow Теневая область flash
 * @return Статус проверки
 */
static data_flash_code data_area_check(data_flash_t main, data_flash_t shadow)
{
	data_flash_code code = data_flash_check_crc(main);
	if(code == FLASH_CHECKSUM_MISMATCH)
	{	
		if((code = data_flash_check_crc(shadow)) != FLASH_OK)
		{
			os_printf("flash: all area is corrupted\n");
		}
		else
		{	//откат main, shadow цел
			code = data_flash_copy_area(main, shadow);
		}
	}
	else if(code == FLASH_OK && (code = data_flash_check_crc(shadow)) == FLASH_CHECKSUM_MISMATCH)
	{	// откат shadow. main цел
		code = data_flash_copy_area(shadow, main);
	}
	return code;
}

/**
 * @brief Функция чтения/Записи данных на flash
 *
 * Данная функци реализует логику работы с основной и теневой областью flash.
 * Сначала производится работа (чтение/запись) с shadow_area, если все ок, то происходит работа с main_area.
 * Если при работе с shadow_area происходит ошибка типа FLASH_CHECKSUM_MISMATCH, то
 * происходит попытка востановление shadow_area из main_area:
 *	- eсли востановление удалось, то происходит повторная попытка записи в shadow_area;
 *	- если востановление не удалось возврощается код возврата.
 *
 * Работа с main_area происходит аналогично shadow_area, запись/чтение в случае FLASH_CHECKSUM_MISMATCH
 * происходит попытка востановления main_area из shadow_area и повторная работа с main_area.
 *
 * @param[in] addr Смещение оносительно начала area
 * @param[in] data Данный для записи или буфер записи полученных данных
 * @param[in] size Размер записываемых данных или размер буфера для записи полученных даннх
 * @param[in] write Флаг указываещий операцию true - запись на flash, false - чтение с flash
 * @return Результат опирации
 */
static data_flash_code data_flash_write_read_data(uint32_t addr, void* data, uint32_t size, bool write)
{
	data_flash_code result = write 
		? data_flash_write(shadow_area, addr, data, size)
		: data_flash_read(shadow_area, addr, data, size);

	if(result == FLASH_CHECKSUM_MISMATCH && data_flash_check_crc(main_area) == FLASH_OK)
	{	// откат shadow. main цел
		result = data_flash_copy_area(shadow_area, main_area);
		if(result == FLASH_OK)
		{
			result = write 
				? data_flash_write(shadow_area, addr, data, size)
				: data_flash_read(shadow_area, addr, data, size);
		}
		else
		{
			os_printf("flash: failed restore shadow area from main\n");
		}
	}

	// shadow_area обработана, работает с main_area
	if(result == FLASH_OK)
	{
		result = write 
			? data_flash_write(main_area, addr, data, size)
			: data_flash_read(main_area, addr, data, size);

		if(result == FLASH_CHECKSUM_MISMATCH && data_flash_check_crc(shadow_area) == FLASH_OK)
		{
			result = data_flash_copy_area(main_area, shadow_area);
			if(result == FLASH_OK)
			{
				result = write 
					? data_flash_write(main_area, addr, data, size)
					: data_flash_read(main_area, addr, data, size);
			}
			else
			{
				os_printf("flash: failed restore main area from shadow\n");
			}
		}
	}
	return result;
}

void data_init_layout()
{
	if(main_area == NULL)
	{
		//0 - смещение относительно DATA_FLASH_BASE_ADDR
		//layout_info.device_list_offset + sizeof(struct data_device_info) * DATA_DEVICE_LIST_SIZE - размер хранимых данных
		main_area = data_flash_init(0, layout_info.device_list_offset + sizeof(struct data_device_info) * DATA_DEVICE_LIST_SIZE);
		if(main_area == NULL)
		{
			os_printf("flash: failed init main_area\n");
		}
		else
		{
			os_printf("flash: main_area data_size: %d, real_size: %d\n", data_flash_get_data_size(main_area), data_flash_get_real_size(main_area));
		}
	}

	if(shadow_area == NULL)
	{
		//data_flash_get_real_size - используется для получения точного размера area с учетом наклодных расходов на хранение crc
		shadow_area = data_flash_init(data_flash_get_real_size(main_area), layout_info.device_list_offset + sizeof(struct data_device_info) * DATA_DEVICE_LIST_SIZE);
		if(shadow_area == NULL)
		{
			os_printf("flash: failed init shadow area\n");
		}
		else
		{
			os_printf("flash: shadow_area data_size: %d, real_size: %d\n", data_flash_get_data_size(shadow_area), data_flash_get_real_size(shadow_area));
		}
	}
}

void data_destroy_layout()
{
	if(main_area != NULL)
	{
		data_flash_destroy(main_area);
	}

	if(shadow_area != NULL)
	{
		data_flash_destroy(shadow_area);
	}
}

bool data_read_custom_name(struct data_custom_name* info)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = data_flash_write_read_data(layout_info.custom_name_offset, (void*) info, DATA_CUSTOM_NAME_SIZE, false);
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

bool data_write_custom_name(struct data_custom_name* info)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = data_flash_write_read_data(layout_info.custom_name_offset, (void*) info, sizeof(struct data_custom_name), true);
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

bool data_device_info_get_powered(struct data_device_info* info)
{
	return info != NULL ? info->device_type >> 7 : 0;
}

uint8_t data_device_info_get_type_int(struct data_device_info* info)
{
	return info != NULL ? info->device_type & 0x0F : 0;
}

const char* data_device_info_get_type(struct data_device_info* info)
{
	uint8_t type = data_device_info_get_type_int(info);
	return type > device_types_map_size ? device_types_map[0] : device_types_map[type];
}

bool data_read_current_device(struct data_device_info* info)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = data_flash_write_read_data(layout_info.current_device_offset, (void*) info, sizeof(struct data_device_info), false);
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

bool data_read_wifi_info(struct data_wifi_info* info, uint32_t index)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL && index < DATA_WIFI_LIST_SIZE)
	{
		uint32_t count = data_get_wifi_info_list_size();
		if(count > index)
		{
			uint32_t offset = layout_info.wifi_list_offset + sizeof(struct data_wifi_info) * index;
			result = data_flash_write_read_data(offset, (void*) info, sizeof(struct data_wifi_info), false);
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
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, DATA_WIFI_LIST_SIZE);
	}
	return result == FLASH_OK;
}

bool data_write_wifi_info(struct data_wifi_info* info, uint32_t index)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL && index < DATA_WIFI_LIST_SIZE)
	{
		uint32_t offset = layout_info.wifi_list_offset + sizeof(struct data_wifi_info) * index;
		result = data_flash_write_read_data(offset, (void*) info, sizeof(struct data_wifi_info), true);
		if(result == FLASH_OK)
		{
			uint32_t count = data_get_wifi_info_list_size();
			if(count == 0 || index > count - 1)
			{
				result = data_set_wifi_info_list_size(index + 1);
			}
		}
		else
		{
			os_printf("flash: failed write wifi_info, result: %d\n", result);
		}
	}
	else
	{
		result = FLASH_OUT_OR_RANGE;
		os_printf("flash: wifi_info is NULL or index: %d out of %d\n", index, DATA_WIFI_LIST_SIZE);
	}
	return result == FLASH_OK;
}

bool data_read_main_wifi(struct data_wifi_info* info)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = data_flash_write_read_data(layout_info.main_wifi_offset, (void*) info, sizeof(struct data_wifi_info), false);
		if(result != FLASH_OK)
		{
			os_printf("flash: failed read main wifi info, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: wifi_info is NULL\n");
	}
	return result == FLASH_OK;
}

bool data_write_main_wifi(struct data_wifi_info* info)
{
	data_flash_code result = FLASH_OK;
	if(info != NULL)
	{
		result = data_flash_write_read_data(layout_info.main_wifi_offset, (void*) info, sizeof(struct data_wifi_info), true);
		if(result != FLASH_OK)
		{
			os_printf("flash: failed write main wifi info, result: %d\n", result);
		}
	}
	else
	{
		os_printf("flash: wifi_info is NULL\n");
	}
	return result == FLASH_OK;
}

bool data_erase_main_wifi()
{
	return data_flash_erase(main_area, layout_info.main_wifi_offset, sizeof(struct data_wifi_info)) == FLASH_OK;
}

uint32_t data_get_wifi_info_list_size()
{
	uint32_t count = 0;
	data_flash_code result = data_flash_write_read_data(layout_info.wifi_list_count_offset, (void*) &count, sizeof(uint32_t), false);
	if(result != FLASH_OK)
	{
		os_printf("flash: failed read wifi info list size, result: %d\n", result);
	}
	return count;
}

static data_flash_code data_set_wifi_info_list_size(uint32_t count)
{
	data_flash_code result = data_flash_write_read_data(layout_info.wifi_list_count_offset, (void*) &count, sizeof(uint32_t), true);
	if(result != FLASH_OK)
	{
		os_printf("flash: failed write wifi info list size, result: %d\n", result);
	}
	return result;
}

/**
 * @}
 *
 * @}
 */


