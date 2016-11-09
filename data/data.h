#ifndef __DATA_H__
#define __DATA_H__

#include "esp_common.h"
#include "data_config.h"

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
 * @brief Структура для хранения данных об устройстве
 */
struct data_device_info
{
	uint8_t device_type;		///< тип устройства
	uint8_t device_id;			///< id устройства
	uint8_t active;				///< активно или нет
	uint8_t reserved;			///< просто байт для выравнивания структуры до 4 байт
};

/**
 * @brief Структура для хранения данных о wifi сети
 */
struct data_wifi_info
{
	char name[DATA_WIFI_NAME_SIZE];			///< имя wifi сети
	char pass[DATA_WIFI_PASS_SIZE];			///< пароль для wifi сети

	uint32_t ip;						///< ip адресс
	uint32_t mask;						///< маска подсети
	uint32_t gw;						///< шлюз для выхода в Internet
	uint32_t dns;						///< dns server
};

/**
 * @brief Структура для хранения имени устройства
 * Хранит только имя текущего сутройства. Возможно имеет смысл закинуть в data_device_info
 */
struct data_custom_name
{
	char data[DATA_CUSTOM_NAME_SIZE];		///< имя устройства
};

/**
 * @brief Функция размечает область flash в зависимости от настроек
 */
void data_init_layout();

/**
 * @brief Функция очищает текущую разметку флеш
 * @note Данные остаются на месте, просто любая функция data api будет возвращять false
 */
void data_destroy_layout();

/**
 * @brief Функция для определения питания утройства
 * @param[in] info Указатель на device_info структуру
 * @return true - питание разеточное, false - батарейки
 */
bool data_device_info_get_powered(struct data_device_info* info);

/**
 * @brief Функция для получения типа устройства
 * @param[in] info Указатель на device_info структуру
 * @return Тип устройства в int
 */
uint8_t data_device_info_get_type_int(struct data_device_info* info);

/**
 * @brief Функция для получения типа устройства в строковом виде
 * @param[in] info Указатель на device_info структуру
 * @return Тип устройства в char
 */
const char* data_device_info_get_type(struct data_device_info* info);

/** 
 * @brief функция для чтения имени устройства
 * Читает с flash имя устройства установленное пользователем
 * @return Результат опирации успех - провал
 */
bool data_read_custom_name(struct data_custom_name* info);

/** 
 * @brief функция для запеси имени устройства
 * Записывает на flash имя устройства заданное пользователем
 * @return Результат опирации успех - провал
 */
bool data_write_custom_name(struct data_custom_name* info);

/**
 * @brief Функия для чтения информации об устройсвте
 * Информация о устройстве прошиваеться 1 раз при прошивке и не
 * меняеться
 * @param[in,out] info Указатель на device_info структуру
 * @return true - все прочиталось, false - нет
 */
bool data_read_current_device(struct data_device_info* info);

/**
 * @brief Функция для чтения с flash информации о wifi сети
 * Планировалось хранить список wifi сетей, но пока не используется
 * @param[in] info Указатель на структуру data_wifi_info
 * @param[in] index Индекс в списке wifi сетей
 * @return true - все прочиталось, false - нет
 */
bool data_read_wifi_info(struct data_wifi_info* info, uint32_t index);

/**
 * @brief Функция для записи на flash информации о wifi сети
 * @param[in] info Указатель на структуру data_wifi_info
 * @param[in] index Индекс в списке wifi сетей
 * @return true - все записалось, false - нет
 */
bool data_write_wifi_info(struct data_wifi_info* info, uint32_t index);
/**
 * @brief Функция для получения размера листа wifi сетей
 */
uint32_t data_get_wifi_info_list_size();

/**
 * @brief Функция для чтения с flash информации о основной wifi сети
 * В текущем SDK выглядит костылем, т.к. в sdk есть функция для указания wifi сети
 * к которой будет происходить авто подключение.
 * @param[in] info Указатель на структуру data_wifi_info
 * @param[in] index Индекс в списке wifi сетей
 * @return true - все прочиталось, false - нет
 */
bool data_read_main_wifi(struct data_wifi_info* info);

/**
 * @brief Функция для записи на flash информации о основной wifi сети
 * @param[in] info Указатель на структуру data_wifi_info
 * @return true - все записалось, false - нет
 */
bool data_write_main_wifi(struct data_wifi_info* info);

/**
 * @brief Функция для стирания с flash информации о основной wifi сети
 * @param[in] info Указатель на структуру data_wifi_info
 * @return true - информация стерта, false - нет
 */
bool data_erase_main_wifi();

/**
 * @}
 *
 * @}
 */

#endif
