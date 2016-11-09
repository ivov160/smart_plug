#ifndef __DATA_CONFIG_H__
#define __DATA_CONFIG_H__

#include "user_data_config.h"

/**
 * @defgroup flash Flash 
 * @brief Работа с данными на flash
 *
 * @defgroup data_api Data API
 * @brief API для работы с данными
 *
 * @addtogroup flash
 * @{
 */

/** 
 * @addtogroup data_api
 * @{
 */

/**
 * @brief Параметр задает масимальный размер массива data_wifi_info хранящийся в 1 area
 */
#define DATA_WIFI_LIST_SIZE 32

/**
 * @brief Параметр задает масимальный размер массива data_device_info хранящийся в 1 area
 */
#define DATA_DEVICE_LIST_SIZE 32

/**
 * @brief Параметр задает маскимальную длинну для имени устройства
 */
#define DATA_CUSTOM_NAME_SIZE 64

/**
 * @brief Параметр задает размер поля name структуры data_wifi_info
 */
#define DATA_WIFI_NAME_SIZE 32

/**
 * @brief Параметр задает размер поля pass структуры data_wifi_info
 */
#define DATA_WIFI_PASS_SIZE 64

/**
 * @}
 */

/** 
 * @addtogroup flash_api
 * @{
 */

/**
 * @brief Параметр задает начальный адресс flash используемой для хранения данных
 * Данный адресс является отправной точкой для разметки flash 
 * data_flash_init расчитывает offset относительно данного адресса
 *
 * @warning Данный адресс взят не случайно для esp с 512mb был правлен ld script:
 * @code{ld}
 *	MEMORY
 *	{
 *		dport0_0_seg :                      	org = 0x3FF00000, len = 0x10
 *		dram0_0_seg :                       	org = 0x3FFE8000, len = 0x18000
 *		iram1_0_seg :                       	org = 0x40100000, len = 0x8000
 *		irom0_0_seg :                       	org = 0x40220000, len = 0x4C000
 *	}
 *	@endcode
 */
#define DATA_FLASH_BASE_ADDR 0x6C000

/**
 * @brief Параметр задает размер flash используемой для хранения даеных
 */
#define DATA_FLASH_SIZE 0xC000

/**
 * @brief Параметр кол-во сегментов внутри одной area
 * @note Данный параметр вычисляется в зависимости от хранимого объема данных, 
 * вычисляется как TOTAL_FLASH_SIZE / FLASH_SEGMENTS - 8byte.
 * Где TOTAL_FLASH_SIZE - необходимый объем памяти для хранения данных
 *
 * Так как используется теневая копия (shadow area),
 * то реальный размер размеченной flash составляет: FLASH_SEGMENTS * 2
 */
/*#define DATA_FLASH_SEGMENTS 4*/

/**
 * @brief Размер сегмента записываемого, читаемого за 1 опирацию
 * Алиас для SPI_FLASH_SEC_SIZE взятой из sdk
 */
#define DATA_FLASH_SEGMENT_SIZE SPI_FLASH_SEC_SIZE

/**
 * @brief Размер ячейки памяти flash в байтах
 */
#define DATA_FLASH_UNIT_SIZE 4

/**
 * @brief Размер буфер для чтения записи секторов (должен быть кратен DATA_FLASH_SEGMENT_SIZE) в байтах
 */
#define DATA_FLASH_BUFFER_SIZE (DATA_FLASH_SEGMENT_SIZE * 2)

/**
 * @}
 */

#endif
