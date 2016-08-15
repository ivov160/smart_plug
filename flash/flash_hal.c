#include "flash_hal.h"

// проверям выравнивание по FLASH_SEGMENT_SIZE
#if FLASH_BASE_ADDR % FLASH_SEGMENT_SIZE != 0
#error "FLASH_BASE_ADDR not alligned for FLASH_SEGMENT_SIZE"
#endif

#if FLASH_BUFFER_SIZE % FLASH_SEGMENT_SIZE != 0
#error "FLASH_BUFFER_SIZE not alligned for FLASH_SEGMENT_SIZE"
#endif

#define FLASH_CRC_SIZE sizeof(uint32_t)
#define FLASH_SECTOR_DATA_SIZE (FLASH_SEGMENT_SIZE - FLASH_CRC_SIZE)
#define FLASH_BUFFER_SECTORS (FLASH_BUFFER_SIZE / FLASH_SEGMENT_SIZE)

/*uint32_t FLASH_CRC_SIZE = sizeof(uint32_t);*/
/*uint32_t FLASH_SECTOR_DATA_SIZE = FLASH_SEGMENT_SIZE - FLASH_CRC_SIZE;*/
/*uint32_t FLASH_BUFFER_SECTORS = (FLASH_BUFFER_SIZE) / FLASH_SEGMENT_SIZE;*/

struct _flash
{
	uint32_t addr;
	uint8_t sectors;
	uint32_t data_size;
	uint32_t real_size;
};

uint32_t crc32(uint32_t crc, const uint8_t *data, uint32_t size)
{
  static const uint32_t s_crc32[16] = { 0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c };

  if (data == NULL)
  {
	  return 0;
  }

  uint32_t crcu32 = ~crcu32; 
  while (size-- > 0) 
  { 
	  uint8_t b = *data++; 
	  crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
	  crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)]; 
  }
  return ~crcu32;
}


uint32_t flash_hal_get_sectors_count(uint32_t size)
{
	// +1 всегда, т.к. есть crc для всего блока памяти

	uint32_t count = 0;
	uint32_t data_size = 0;

	/*count =  size / (4 * 1024 - 4) + 1;*/
	count =  size / FLASH_SECTOR_DATA_SIZE + 1;
	while((data_size = count * FLASH_SECTOR_DATA_SIZE - FLASH_CRC_SIZE) < size)
	{	// во внутрь зайти не должно никогда, но shit happens
		++count;
	}
	return count;
}

uint32_t private_flash_hal_get_real_size(uint32_t size)
{
	return flash_hal_get_sectors_count(size) * FLASH_SEGMENT_SIZE;
}

uint32_t private_flash_hal_get_data_size(uint32_t size)
{
	return flash_hal_get_sectors_count(size) * FLASH_SECTOR_DATA_SIZE - FLASH_CRC_SIZE;
}

flash_code flash_hal_check_sector_crc(uint8_t *data, uint32_t size, uint32_t crc)
{
	flash_code code = FLASH_OK;
	///@todo удалить заглушку, после того, как crc будет нормально записываться
	if(crc != 0xFFFFFFFF)
	{
		uint32_t current_crc = crc32(0, data, size);
		if(current_crc != crc)
		{
			os_printf("flash: crc32 mismatch, current crc: 0x%x, sector crc: 0x%x\n", current_crc, crc);
			code = FLASH_CHECKSUM_MISMATCH;
		}
	}
	return code;
}

uint32_t flash_hal_get_data_size(flash_t handle)
{
	uint32_t size = 0;
	if(handle != NULL)
	{
		size = handle->data_size;
	}
	return size;
}

uint32_t flash_hal_get_real_size(flash_t handle)
{
	uint32_t size = 0;
	if(handle != NULL)
	{
		size = handle->real_size;
	}
	return size;
}

flash_t flash_hal_init(uint32_t offset, uint32_t size)
{
	/*os_printf("flash: FLASH_SIZE: %d, FLASH_SEGMENT_SIZE: %d, FLASH_BUFFER_SIZE: %d, FLASH_SECTOR_DATA_SIZE: %d, FLASH_CRC_SIZE: %d\n", FLASH_SIZE, FLASH_SEGMENT_SIZE, FLASH_BUFFER_SIZE, FLASH_SECTOR_DATA_SIZE, FLASH_CRC_SIZE);*/

	flash_t handle = NULL;
	if(offset % FLASH_SEGMENT_SIZE == 0)
	{
		uint32_t real_size = private_flash_hal_get_real_size(size);
		if(real_size < FLASH_SIZE)
		{
			handle = (flash_t)malloc(sizeof(struct _flash));	
			handle->addr = FLASH_BASE_ADDR + offset;
			handle->sectors = flash_hal_get_sectors_count(size);
			handle->data_size = private_flash_hal_get_data_size(size);
			handle->real_size = real_size;
		}
		else
		{
			os_printf("flash: size overflow flash area, size: %d, real_size: %d, area_size: %d\n", size, real_size, FLASH_SIZE);
		}
	}
	else
	{
		os_printf("flash: offset not alligned for FLASH_SEGMENT_SIZE\n");
	}
	return handle;
}

void flash_hal_destroy(flash_t handle)
{
	if(handle != NULL)
	{
		free(handle);
	}
}


flash_code flash_hal_get_sectors_range(flash_t handle, uint32_t offset, uint32_t size, struct flash_sectors_range* range)
{
	flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		uint32_t sectors_count = flash_hal_get_sectors_count(offset + size);
		if(handle->data_size > offset + size && handle->sectors >= sectors_count)
		{
			range->first_index = offset / FLASH_SECTOR_DATA_SIZE;
			range->count = sectors_count;
			range->last_index = range->first_index + sectors_count - 1;
		}
		else
		{
			code = FLASH_OUT_OR_RANGE;
		}
	}
	return code;
}

flash_code flash_hal_read(flash_t handle, uint32_t offset, void* data, uint32_t size)
{
	flash_code code = handle != NULL ? flash_hal_check_crc(handle) : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		struct flash_sectors_range range;
		if((code = flash_hal_get_sectors_range(handle, offset, size, &range)) == FLASH_OK)
		{
			uint8_t* buffer = (uint8_t*)malloc(FLASH_BUFFER_SIZE);

			uint32_t total_read_size = 0;
			uint32_t sector = range.first_index;
			uint32_t sectors_left = range.count;

			while(sectors_left > 0)
			{
				// сколько читать секторов
				uint32_t sectors_read = sectors_left < FLASH_BUFFER_SECTORS ? sectors_left : FLASH_BUFFER_SECTORS;
				uint32_t aligned_addr = (handle->addr + sector * FLASH_SEGMENT_SIZE) & (-FLASH_UNIT_SIZE);
				uint32_t total_size = sectors_read * FLASH_SEGMENT_SIZE;

				os_printf("flash: base addr: 0x%x, addr: 0x%x, current sector: %d, need read sectors: %d, size: %d\n", handle->addr, aligned_addr, sector, sectors_read, total_size);

				memset(buffer, 0, FLASH_BUFFER_SIZE);
				if((code = (spi_flash_read(
						aligned_addr, 
						(uint32_t*)buffer, 
						total_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK)) != FLASH_OK)
				{	// не удалось прочитать flash
					break;
				}

				// работаем с буфером как с секторами
				for(uint32_t j = 0; j < sectors_read; ++j)
				{
					uint32_t sector_begin_offset = j * FLASH_SEGMENT_SIZE;
					uint32_t sector_end_offset = j * FLASH_SEGMENT_SIZE + FLASH_SEGMENT_SIZE - 1;

					// забрасываем сеетор для подсчета crc
					// подпровляем оффсет для crc если бы buffer был uint32_t*
					if((code = flash_hal_check_sector_crc(
									buffer + sector_begin_offset, 
									FLASH_SECTOR_DATA_SIZE, 
									((uint32_t*)buffer)[sector_end_offset / sizeof(uint32_t)])))
					{
						break;
					}
					
					// вычисление смещения начала данных для сектора
					// вычисление размера данных для чтения в данном секторе
					uint32_t data_sector_offset = 0;
					uint32_t data_sector_size = 0;

					if(sector == range.first_index && j == 0)
					{
						data_sector_offset = offset - sector * FLASH_SEGMENT_SIZE;
						data_sector_size = size > FLASH_SECTOR_DATA_SIZE - data_sector_offset
							? FLASH_SECTOR_DATA_SIZE - data_sector_offset
							: size;
					}
					else
					{
						data_sector_size = size - total_read_size < FLASH_SECTOR_DATA_SIZE
							? size - total_read_size
							: FLASH_SECTOR_DATA_SIZE;
					}
					uint32_t buffer_offset = sector_begin_offset + data_sector_offset;

					// копирование данных из буфера на ружу
					memcpy((uint8_t*) data + (total_read_size / FLASH_UNIT_SIZE), buffer + buffer_offset, data_sector_size);
					total_read_size += data_sector_size;
				}

				// выход из цикла чтения, что-то не так 
				// при копировании данных
				if(code != FLASH_OK)
				{
					break;
				}
				// следующий сектор для чтения
				sector += sectors_read;
				sectors_left -= sectors_read;
			}

			free(buffer);
		}
	}
	return code;
}

flash_code flash_hal_write(flash_t handle, uint32_t offset, void* data, uint32_t size)
{
	flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
	}
	return code;
}

flash_code flash_hal_erase(flash_t handle, uint32_t offset, void* data, uint32_t size)
{
	flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
	}
	return code;
}

flash_code flash_hal_check_crc(flash_t handle)
{
	flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	/*if(code == FLASH_OK)*/
	/*{*/
	/*}*/
	return code;
}
