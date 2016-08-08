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

/**
 * @brief Функция для определения питания утройства
 * @param[in] info Указатель на device_info структуру
 * @return true - питание разеточное, false - батарейки
 */
bool device_info_get_powered(struct device_info* info);

/**
 * @brief Функция для получения типа устройства
 * @param[in] info Указатель на device_info структуру
 * @return Тип устройства в int
 */
uint8_t device_info_get_type_int(struct device_info* info);

/**
 * @brief Функция для получения типа устройства в строковом виде
 * @param[in] info Указатель на device_info структуру
 * @return Тип устройства в char
 */
const char* device_info_get_type(struct device_info* info);

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

/**
 * @brief Функия для чтения информации об устройсвте
 * Информация о устройстве прошиваеться 1 раз при прошивке и не
 * меняеться
 * @param[in,out] info Указатель на device_info структуру
 * @return true - все прочиталось, false - нет
 */
bool read_current_device(struct device_info* info);

bool read_wifi_info(struct wifi_info* info, uint32_t index);
bool write_wifi_info(struct wifi_info* info, uint32_t index);
uint32_t get_wifi_info_list_size();
/*bool erase_wifi_info(uint32_t index);*/
/*bool exist_wifi_info(uint32_t index);*/

/*bool read_device_info(struct device_info* info, uint32_t index);*/
/*bool write_device_info(struct device_info* info, uint32_t index);*/
/*bool exist_device_info(uint32_t index);*/
/*bool erase_device_info(uint32_t index);*/

#endif
