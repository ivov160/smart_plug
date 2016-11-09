#include "flash.h"
#include "flash_utils.h"

// проверям выравнивание по DATA_FLASH_SEGMENT_SIZE
#if DATA_FLASH_BASE_ADDR % DATA_FLASH_SEGMENT_SIZE != 0
#error "DATA_FLASH_BASE_ADDR not alligned for DATA_FLASH_SEGMENT_SIZE"
#endif

#if DATA_FLASH_BUFFER_SIZE % DATA_FLASH_SEGMENT_SIZE != 0
#error "DATA_FLASH_BUFFER_SIZE not alligned for DATA_FLASH_SEGMENT_SIZE"
#endif

#define DATA_FLASH_CRC_SIZE sizeof(uint32_t)
#define DATA_FLASH_SECTOR_DATA_SIZE (DATA_FLASH_SEGMENT_SIZE - DATA_FLASH_CRC_SIZE - DATA_FLASH_CRC_SIZE)
#define DATA_FLASH_BUFFER_SECTORS (DATA_FLASH_BUFFER_SIZE / DATA_FLASH_SEGMENT_SIZE)

struct _flash
{
	uint32_t addr;
	int32_t sectors;
	uint32_t data_size;
	uint32_t real_size;
};


struct data_flash_sectors_range
{
	uint32_t first_index;
	uint32_t last_index;
	uint32_t count;
};


static data_flash_code data_flash_read_write(data_flash_t handle, uint32_t offset, void* data, uint32_t size, bool write);
static data_flash_code private_flash_check_crc(data_flash_t handle, bool write);
static uint32_t data_flash_calc_sector_crc(uint8_t *data, uint32_t size);
static data_flash_code data_flash_get_sectors_range(data_flash_t handle, uint32_t offset, uint32_t size, struct data_flash_sectors_range* range);

uint32_t data_flash_get_sectors_count(uint32_t size)
{
	// +1 всегда, т.к. есть crc для всего блока памяти

	uint32_t count = 0;
	uint32_t data_size = 0;

	/*count =  size / (4 * 1024 - 4) + 1;*/
	count =  size / DATA_FLASH_SECTOR_DATA_SIZE + 1;
	while((data_size = count * DATA_FLASH_SECTOR_DATA_SIZE - DATA_FLASH_CRC_SIZE) < size)
	{	// во внутрь зайти не должно никогда, но shit happens
		++count;
	}
	return count;
}

uint32_t private_flash_get_real_size(uint32_t size)
{
	return data_flash_get_sectors_count(size) * DATA_FLASH_SEGMENT_SIZE;
}

uint32_t private_flash_get_data_size(uint32_t size)
{
	return data_flash_get_sectors_count(size) * DATA_FLASH_SECTOR_DATA_SIZE - DATA_FLASH_CRC_SIZE;
}

data_flash_code data_flash_check_sector_crc(uint8_t *data, uint32_t size, uint32_t crc)
{
	data_flash_code code = FLASH_OK;
	///@todo удалить заглушку, после того, как crc будет нормально записываться
	if(crc != 0xFFFFFFFF)
	{
		uint32_t current_crc = data_flash_calc_sector_crc(data, size);
		if(current_crc != crc)
		{
			os_printf("flash: crc32 mismatch, current crc: 0x%x, sector crc: 0x%x\n", current_crc, crc);
			code = FLASH_CHECKSUM_MISMATCH;
		}
	}
	return code;
}

uint32_t data_flash_get_data_size(data_flash_t handle)
{
	uint32_t size = 0;
	if(handle != NULL)
	{
		size = handle->data_size;
	}
	return size;
}

uint32_t data_flash_get_real_size(data_flash_t handle)
{
	uint32_t size = 0;
	if(handle != NULL)
	{
		size = handle->real_size;
	}
	return size;
}

data_flash_t data_flash_init(uint32_t offset, uint32_t size)
{
	/*os_printf("flash: DATA_FLASH_SIZE: %d, DATA_FLASH_SEGMENT_SIZE: %d, DATA_FLASH_BUFFER_SIZE: %d, DATA_FLASH_SECTOR_DATA_SIZE: %d, DATA_FLASH_CRC_SIZE: %d\n", 
	 * DATA_FLASH_SIZE, DATA_FLASH_SEGMENT_SIZE, DATA_FLASH_BUFFER_SIZE, DATA_FLASH_SECTOR_DATA_SIZE, DATA_FLASH_CRC_SIZE);*/

	data_flash_t handle = NULL;
	if(offset % DATA_FLASH_SEGMENT_SIZE == 0)
	{
		uint32_t real_size = private_flash_get_real_size(size);
		if(real_size < DATA_FLASH_SIZE)
		{
			handle = (data_flash_t)malloc(sizeof(struct _flash));	
			handle->addr = DATA_FLASH_BASE_ADDR + offset;
			handle->sectors = data_flash_get_sectors_count(size);
			handle->data_size = private_flash_get_data_size(size);
			handle->real_size = real_size;
		}
		else
		{
			os_printf("flash: size overflow flash area, size: %d, real_size: %d, area_size: %d\n", size, real_size, DATA_FLASH_SIZE);
		}
	}
	else
	{
		os_printf("flash: offset not alligned for DATA_FLASH_SEGMENT_SIZE\n");
	}
	return handle;
}

void data_flash_destroy(data_flash_t handle)
{
	if(handle != NULL)
	{
		free(handle);
	}
}


data_flash_code data_flash_read(data_flash_t handle, uint32_t offset, void* data, uint32_t size)
{
	return data_flash_read_write(handle, offset, data, size, false);
}

data_flash_code data_flash_write(data_flash_t handle, uint32_t offset, void* data, uint32_t size)
{
	return data_flash_read_write(handle, offset, data, size, true);
}

static data_flash_code data_flash_read_write(data_flash_t handle, uint32_t offset, void* data, uint32_t size, bool write)
{
	data_flash_code code = handle != NULL ? data_flash_check_crc(handle) : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		struct data_flash_sectors_range range;
		if((code = data_flash_get_sectors_range(handle, offset, size, &range)) == FLASH_OK)
		{
			uint8_t* buffer = (uint8_t*)malloc(DATA_FLASH_BUFFER_SIZE);

			uint32_t total_size = 0;
			uint32_t sector = range.first_index;
			int32_t sectors_left = range.count;

			while(sectors_left > 0)
			{	 // сколько читать секторов
				uint32_t sectors_read = sectors_left < DATA_FLASH_BUFFER_SECTORS ? sectors_left : DATA_FLASH_BUFFER_SECTORS;
				uint32_t aligned_addr = (handle->addr + sector * DATA_FLASH_SEGMENT_SIZE) & (-DATA_FLASH_UNIT_SIZE);
				uint32_t read_size = sectors_read * DATA_FLASH_SEGMENT_SIZE;

				os_printf("flash: base addr: 0x%x, addr: "
						  "0x%x, current sector: %d, need "
						  "read sectors: %d, size: %d, write: %s\n",
						  handle->addr, aligned_addr,
						  sector, sectors_read, read_size, (write ? "true" : "false"));

				memset(buffer, 0, DATA_FLASH_BUFFER_SIZE);
				code = (spi_flash_read(
						aligned_addr, 
						(uint32_t*)buffer, 
						read_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK);

				if(code != FLASH_OK)
				{	// не удалось прочитать flash
					break;
				}

				// работаем с буфером как с секторами
				for(uint32_t j = 0; j < sectors_read; ++j)
				{
					int32_t sector_begin_offset = j * DATA_FLASH_SEGMENT_SIZE;
					// crc сектора записанно в предпоследнем элементе сегмента (DATA_FLASH_SEGMENT_SIZE)
					// в последнем элементе сектора записанн crc area, если сегмент крайний в area, иначе записываеться 0xFFFFFFFF
					uint32_t sector_crc_offset = sector_begin_offset + DATA_FLASH_SECTOR_DATA_SIZE;

					// забрасываем сеетор для подсчета crc
					// подпровляем оффсет для crc если бы buffer был uint32_t*
					if((code = data_flash_check_sector_crc(
									buffer + sector_begin_offset, 
									DATA_FLASH_SECTOR_DATA_SIZE, 
									*((uint32_t*)(buffer + sector_crc_offset)))))
					{
						break;
					}
					
					// вычисление смещения начала данных для сектора
					// вычисление размера данных для чтения в данном секторе
					uint32_t data_sector_offset = 0;
					uint32_t data_sector_size = 0;

					if(sector == range.first_index && j == 0)
					{
						data_sector_offset = offset - sector * DATA_FLASH_SEGMENT_SIZE;
						data_sector_size = size > DATA_FLASH_SECTOR_DATA_SIZE - data_sector_offset
							? DATA_FLASH_SECTOR_DATA_SIZE - data_sector_offset
							: size;
					}
					else
					{
						data_sector_size = size - total_size < DATA_FLASH_SECTOR_DATA_SIZE
							? size - total_size
							: DATA_FLASH_SECTOR_DATA_SIZE;
					}
					uint32_t buffer_offset = sector_begin_offset + data_sector_offset;

					if(write)
					{	// копирование данных в буфер 
						memcpy(buffer + buffer_offset, (uint8_t*) data + total_size, data_sector_size);
						uint32_t new_crc = data_flash_calc_sector_crc(buffer + sector_begin_offset, DATA_FLASH_SECTOR_DATA_SIZE);
						*((uint32_t*)(buffer + sector_crc_offset)) = new_crc;

						// sector + j потому, что sector начало + j текущий сектор в буфере
						// стираем сразу т.к. всеравно по секторно
						uint32_t flash_sector_addr = handle->addr + (sector + j *  DATA_FLASH_SEGMENT_SIZE);

						code = (spi_flash_erase_sector(flash_sector_addr / DATA_FLASH_SEGMENT_SIZE) != SPI_FLASH_RESULT_OK 
										? FLASH_SPI_ERROR 
										: FLASH_OK);

						if(code != FLASH_OK)
						{	// не удалось стересть сектор на flash
							break;
						}
					}
					else
					{	// копирование данных из буфера на ружу
						memcpy((uint8_t*) data + total_size, buffer + buffer_offset, data_sector_size);
					}

					total_size += data_sector_size;
				}

				// выход из цикла, что-то не так 
				// при копировании данных
				if(code != FLASH_OK)
				{
					break;
				}

				if(write)
				{
					code = (spi_flash_write(
							aligned_addr, 
							(uint32_t*)buffer, 
							read_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK);

					if(code != FLASH_OK)
					{	// не удалось записать buffer на flash
						break;
					}
				}

				// следующий сектор для чтения
				sector += sectors_read;
				sectors_left -= sectors_read;
			}
			free(buffer);

			if(write)
			{
				code = private_flash_check_crc(handle, true);
			}
		}
	}
	return code;
}

data_flash_code flash_erase(data_flash_t handle, uint32_t offset, uint32_t size)
{
	data_flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		uint8_t* data = (uint8_t*) malloc(size);
		memset(data, 0xFF, size);
		code = data_flash_write(handle, offset, (void*) data, size);
		free(data);
	}
	return code;
}

data_flash_code data_flash_check_crc(data_flash_t handle)
{
	return private_flash_check_crc(handle, false);
}

static data_flash_code private_flash_check_crc(data_flash_t handle, bool write)
{
	data_flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		uint32_t area_crc = 0;
		uint32_t current_crc = 0;

		uint32_t* crc_buffer = (uint32_t*) malloc(DATA_FLASH_CRC_SIZE * 2);
		for(uint32_t sector = 0; sector < handle->sectors; ++sector)
		{	
			uint32_t aligned_addr = (handle->addr + sector * DATA_FLASH_SEGMENT_SIZE + DATA_FLASH_SECTOR_DATA_SIZE) & (-DATA_FLASH_UNIT_SIZE);
			uint32_t crc_read_size = sector == handle->sectors - 1 ? DATA_FLASH_CRC_SIZE * 2 : DATA_FLASH_CRC_SIZE;

			memset(crc_buffer, 0, DATA_FLASH_CRC_SIZE * 2);
			code = (spi_flash_read(
					aligned_addr,
					crc_buffer,
					crc_read_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK);

			if(code != FLASH_OK)
			{	// не удалось прочитать flash
				break;
			}

			current_crc = crc32(current_crc, (uint8_t*) crc_buffer, DATA_FLASH_CRC_SIZE);
			if(sector == handle->sectors - 1)
			{
				area_crc = crc_buffer[1];
			}
		}
		free(crc_buffer);

		if(write)
		{	// записываем новую сумму для area
			uint32_t aligned_addr = (handle->addr + (handle->sectors - 1) * DATA_FLASH_SEGMENT_SIZE) & (-DATA_FLASH_UNIT_SIZE);
			uint8_t* sector_buffer = (uint8_t*) zalloc(DATA_FLASH_SEGMENT_SIZE);
			if((code = spi_flash_read(
					aligned_addr,
					(uint32_t*) sector_buffer,
					DATA_FLASH_SEGMENT_SIZE) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK) == FLASH_OK)
			{
				uint32_t sector_crc_offset = DATA_FLASH_SECTOR_DATA_SIZE + DATA_FLASH_CRC_SIZE;
				*((uint32_t*)(sector_buffer + sector_crc_offset)) = current_crc;

				code = spi_flash_erase_sector(aligned_addr / DATA_FLASH_SEGMENT_SIZE) != SPI_FLASH_RESULT_OK
						? FLASH_SPI_ERROR : FLASH_OK;

				if(code == FLASH_OK)
				{
					code = spi_flash_write(aligned_addr, (uint32_t*) sector_buffer, DATA_FLASH_SEGMENT_SIZE) != SPI_FLASH_RESULT_OK
						? FLASH_SPI_ERROR : FLASH_OK;
				}
				else
				{
					os_printf("flash: erase sector failed addr: %x, sector: %d\n", aligned_addr, aligned_addr / DATA_FLASH_SEGMENT_SIZE);
				}
			}
			free(sector_buffer);
		}
		///@todo удалить заглушку, после того, как crc будет нормально записываться
		else if(area_crc != 0xFFFFFFFF)
		{
			code = area_crc != current_crc ? FLASH_CHECKSUM_MISMATCH : FLASH_OK;
		}
	}
	return code;
}

static uint32_t data_flash_calc_sector_crc(uint8_t *data, uint32_t size)
{
	uint32_t crc = 0;
	if(data != NULL && size != 0)
	{
		crc = crc32(0, data, size);
	}
	else
	{
		os_printf("flash: calc sector crc failed, data or size is null\n");
	}
	return crc;
}

/**
 * @brief Функция для вычисления диапазона секторов для данного объема памяти
 *
 */
static data_flash_code data_flash_get_sectors_range(data_flash_t handle, uint32_t offset, uint32_t size, struct data_flash_sectors_range* range)
{
	data_flash_code code = handle != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		uint32_t sectors_count = data_flash_get_sectors_count(offset + size);
		if(handle->data_size > offset + size && handle->sectors >= sectors_count)
		{
			range->first_index = offset / DATA_FLASH_SECTOR_DATA_SIZE;
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

data_flash_code data_flash_copy_area(data_flash_t dst, data_flash_t src)
{
	data_flash_code code = src != NULL && dst != NULL ? FLASH_OK : FLASH_INVALID_HANDLE;
	if(code == FLASH_OK)
	{
		if(src->sectors == dst->sectors)
		{
			uint8_t* buffer = (uint8_t*)malloc(DATA_FLASH_BUFFER_SIZE);

			int32_t sector = 0;
			int32_t sectors_left = src->sectors;

			while(sectors_left > 0)
			{
				uint32_t sectors_read = sectors_left < DATA_FLASH_BUFFER_SECTORS ? sectors_left : DATA_FLASH_BUFFER_SECTORS;
				uint32_t src_aligned_addr = (src->addr + sector * DATA_FLASH_SEGMENT_SIZE) & (-DATA_FLASH_UNIT_SIZE);
				uint32_t read_size = sectors_read * DATA_FLASH_SEGMENT_SIZE;

				code = (spi_flash_read(
							src_aligned_addr, 
							(uint32_t*)buffer, 
							read_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK);

				if(code != FLASH_OK)
				{
					break;
				}

				for(uint32_t j = 0; j < sectors_read; ++j)
				{
					uint32_t flash_sector_addr = dst->addr + (sector + j *  DATA_FLASH_SEGMENT_SIZE);
					code = (spi_flash_erase_sector(flash_sector_addr / DATA_FLASH_SEGMENT_SIZE) != SPI_FLASH_RESULT_OK 
									? FLASH_SPI_ERROR 
									: FLASH_OK);

					if(code != FLASH_OK)
					{
						break;
					}
				}

				if(code != FLASH_OK)
				{
					break;
				}

				uint32_t dst_aligned_addr = (dst->addr + sector * DATA_FLASH_SEGMENT_SIZE) & (-DATA_FLASH_UNIT_SIZE);
				code = (spi_flash_write(
						dst_aligned_addr, 
						(uint32_t*)buffer, 
						read_size) != SPI_FLASH_RESULT_OK ? FLASH_SPI_ERROR : FLASH_OK);

				if(code != FLASH_OK)
				{	// не удалось записать buffer на flash
					break;
				}

				// следующий сектор для чтения
				sector += sectors_read;
				sectors_left -= sectors_read;
			}
			free(buffer);
		}
		else
		{
			os_printf("flash: copy area failed, area sectors mismatch src: %d, dst: %d\n", src->sectors, dst->sectors);
		}
	}
	return code;
}

