#ifndef __FLASH_HAL_H__
#define __FLASH_HAL_H__

#include "esp_common.h"

struct _flash;
typedef struct _flash* flash_t;

// базовый адрес для flash 
// данный адрес является отпровной точкой для инициалицации flash_t 
// flash_init расчитывает offset относительно данного адресса
#define FLASH_BASE_ADDR 0x6C000
#define FLASH_SIZE 0xC000

// размер сегмента для записи
#define FLASH_SEGMENT_SIZE SPI_FLASH_SEC_SIZE
#define FLASH_UNIT_SIZE 4

// буфер для чтения записи секторов (должен быть кратен FLASH_SEGMENT_SIZE)
#define FLASH_BUFFER_SIZE (FLASH_SEGMENT_SIZE * 2)
/*const uint32_t FLASH_BUFFER_SIZE = FLASH_SEGMENT_SIZE * 2;*/

struct flash_sectors_range
{
	uint32_t first_index;
	uint32_t last_index;
	uint32_t count;
};

typedef enum {
	FLASH_OK = 0,
	FLASH_INVALID_HANDLE = 1,
	FLASH_OUT_OR_RANGE = 2,
	FLASH_CHECKSUM_MISMATCH = 3,
	FLASH_SPI_ERROR = 4,
	FLASH_INVALID_ADDR = 5
} flash_code;

/**
 * @brief Инцилизирует handle для работы с flash
 *
 * Данная функция не смотрит пересечения данного блока с уже выделенными
 * Данный функционал возложен на клиентский код
 *
 * Контролируется только выход за FLASH_END_ADDR = FLASH_BASE_ADDR + FLASH_SIZE
 *
 * Реальный размер будет чуть меньше на (FLASH_SEGMENT_SIZE * sizeof(uin32_t) + sizeof(uin32_t))
 * 
 * @param base_addr Базовый адрес болка flash. Относительно начала flash
 * @param size Размер блока
 *
 * @return Handle (flash_t) для работы с данным блоком NULL - не удалось создать handle
 */
flash_t flash_hal_init(uint32_t offset, uint32_t size);

/**
 * @brief Функция для уничтожения handle'а
 *
 * @param falsh Handle ассоциированный с блоком flash
 *
 */
void flash_hal_destroy(flash_t handle);

/**
 * @brief Функция для вычисления диапазона секторов для данного объема памяти
 *
 */
flash_code flash_hal_get_sectors_range(flash_t handle, uint32_t offset, uint32_t size, struct flash_sectors_range* range);

/**
 * @brief Функция для получения доступного размера под данные в area
 *
 */
uint32_t flash_hal_get_data_size(flash_t handle);

/**
 * @brief Функция для получения реального размера area
 *
 */
uint32_t flash_hal_get_real_size(flash_t handle);

/**
 * @brief Функция для проверки целостности блока flash
 *
 */
flash_code flash_hal_check_crc(flash_t handle);

/**
 * @brief Функция для подсчета и записи crc для area
 *
 */
flash_code flash_hal_check_crc(flash_t handle);

/**
 * @brief Функция для чтения данных с flash
 */
flash_code flash_hal_read(flash_t handle, uint32_t offset, void* data, uint32_t size);

/**
 * @brief Функция для записи данных на flash
 */
flash_code flash_hal_write(flash_t handle, uint32_t offset, void* data, uint32_t size);

/**
 * @brief Функция для стирания данных с flash
 *
 * @note По сути это write с 0xFFFFFFFF
 *
 */
flash_code flash_hal_erase(flash_t handle, uint32_t offset, uint32_t size);





#endif
