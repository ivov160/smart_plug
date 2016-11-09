#ifndef __FLASH_HAL_H__
#define __FLASH_HAL_H__

#include "esp_common.h"
#include "data_config.h"

/**
 * @defgroup flash Flash 
 * @brief Работа с данными на flash
 *
 * @defgroup flash_api 
 * @brief API для работы с флеш
 */


/** 
 * @addtogroup flash
 * @{
 *
 * @addtogroup flash_api
 * @{
 */

/**
 * @brief Handle для работы с размеченной областью flash
 */
struct _data_flash;
typedef struct _flash* data_flash_t;


typedef enum {
	FLASH_OK = 0,
	FLASH_INVALID_HANDLE = 1,
	FLASH_OUT_OR_RANGE = 2,
	FLASH_CHECKSUM_MISMATCH = 3,
	FLASH_SPI_ERROR = 4,
	FLASH_INVALID_ADDR = 5,
	FLASH_INVALID_POINTER = 6
} data_flash_code;

/**
 * @brief Инцилизирует handle для работы с flash
 *
 * Данная функция не смотрит пересечения данного блока с уже выделенными
 * Данный функционал возложен на клиентский код
 *
 * Контролируется только выход за FLASH_END_ADDR = DATA_FLASH_BASE_ADDR + DATA_FLASH_SIZE
 *
 * Реальный размер будет чуть меньше на (DATA_FLASH_SEGMENT_SIZE * sizeof(uin32_t) + sizeof(uin32_t))
 * 
 * @param base_addr Базовый адрес болка flash. Относительно начала flash
 * @param size Размер блока
 *
 * @return Handle (data_flash_t) для работы с данным блоком NULL - не удалось создать handle
 */
data_flash_t data_flash_init(uint32_t offset, uint32_t size);

/**
 * @brief Функция для уничтожения handle'а
 *
 * @param falsh Handle ассоциированный с блоком flash
 *
 */
void data_flash_destroy(data_flash_t handle);

/**
 * @brief Функция для получения доступного размера под данные в area
 *
 */
uint32_t data_flash_get_data_size(data_flash_t handle);

/**
 * @brief Функция для получения реального размера area
 *
 */
uint32_t data_flash_get_real_size(data_flash_t handle);

/**
 * @brief Функция для проверки целостности блока flash
 *
 */
data_flash_code data_flash_check_crc(data_flash_t handle);

/**
 * @brief Функция для чтения данных с flash
 */
data_flash_code data_flash_read(data_flash_t handle, uint32_t offset, void* data, uint32_t size);

/**
 * @brief Функция для записи данных на flash
 */
data_flash_code data_flash_write(data_flash_t handle, uint32_t offset, void* data, uint32_t size);

/**
 * @brief Функция для стирания данных с flash
 *
 * @note По сути это write с 0xFFFFFFFF
 *
 */
data_flash_code data_flash_erase(data_flash_t handle, uint32_t offset, uint32_t size);

/**
 * @brief Функция для копирования одной арии в другую
 */
data_flash_code data_flash_copy_area(data_flash_t dst, data_flash_t src);

/**
 * @}
 *
 * @}
 */


#endif
