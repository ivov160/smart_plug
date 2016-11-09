#ifndef __FLASH_UTILS_H__
#define __FLASH_UTILS_H__

#include "esp_common.h"

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
 * @brief Функция для подсчета crc32
 */
uint32_t crc32(uint32_t crc, const uint8_t *data, uint32_t size);

/**
 * @}
 *
 * @}
 */

#endif
