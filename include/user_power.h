#ifndef __USER_POWER_H__
#define __USER_POWER_H__

#include "esp_common.h"

/**
 * @defgroup user User 
 * @defgroup user_power User power
 * @brief Пользовательский код для работы с нагрузкой
 *
 * @addtogroup user
 * @{
 * @addtogroup user_power 
 * @{
 */

/**
 * @brief Функция для инициализации управления питанием
 */
void power_init();

/**
 * @brief Функция для отключения питания
 */
void power_down();

/**
 * @brief Функция для подачи питания
 *
 */
void power_up();

/**
 * @brief Функция для получения статуса питания
 * @return состояние питания true - питание есть, false - нет
 */
int8_t power_status();

/**
 * @brief Функция запуска тестового режима
 * В тестовом режиме нагрузка постоянно переключается on/off с указанной переодичностью
 * @param[in] ms Время переключения нагрузки в ms
 */
void power_start_test_mode(uint32_t ms);

/**
 * @brief Функция для остановки тестового режима работы
 */
void power_stop_test_mode();

/**
 * @}
 * @}
 */


#endif
