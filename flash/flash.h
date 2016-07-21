#ifndef __FLASH_H__
#define __FLASH_H__

#include "esp_common.h"

#define WIFI_LIST_SIZE 8
#define DEVICE_LIST_SIZE 32

#define CUSTOM_NAME_SIZE 64
#define WIFI_NAME_SIZE 256
#define WIFI_PASS_SIZE 128

#define FLASH_BASE_ADDR 0x6C000
#define FLASH_SIZE 0xC000

struct device_info
{
	uint8_t device_type;
	uint8_t device_id;
	uint8_t active;
	uint8_t reserved;
};

struct wifi_info
{
	char name[WIFI_NAME_SIZE];
	char pass[WIFI_PASS_SIZE];

	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
	uint32_t dns;
};

struct custom_name
{
	char data[CUSTOM_NAME_SIZE];
};

struct layout_meta_info
{
	uint32_t custom_name_offset;
	uint32_t custom_name_addr;

	uint32_t current_device_offset;
	uint32_t current_device_addr;

	uint32_t wifi_list_offset;
	uint32_t wifi_list_addr;

	uint32_t device_list_offset;
	uint32_t device_list_addr;
};

/**
 * @brief Возврощает информацию о flash
 * @return Указатель на раскройку карты (указатель возарощается на статический объект)
 */
struct layout_meta_info* get_layout_info();

/**
 * @brief Функция для сброса всей карты памяти
 * @return Результат опирации успех - провал
 */
bool erase_layout();

/** 
 * @brief функция для чтения имени устройства
 * Читает с flash имя устройства установленное пользователем
 * @return Результат опирации успех - провал
 */
bool read_custom_name(struct custom_name* info);
/** 
 * @brief функция для запеси имени устройства
 * Записывает на flash имя устройства заданное пользователем
 * @return Результат опирации успех - провал
 */
bool write_custom_name(struct custom_name* info);

bool read_current_device(struct device_info* info);
bool write_current_device(struct device_info* info);

bool read_wifi_info(struct wifi_info* info, uint32_t index);
bool write_wifi_info(struct wifi_info* info, uint32_t index);
/*bool exist_wifi_info(uint32_t index);*/
/*bool erase_wifi_info(uint32_t index);*/

/*bool read_device_info(struct device_info* info, uint32_t index);*/
/*bool write_device_info(struct device_info* info, uint32_t index);*/
/*bool exist_device_info(uint32_t index);*/
/*bool erase_device_info(uint32_t index);*/

#endif
