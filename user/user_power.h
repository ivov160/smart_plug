#ifndef __USER_POWER_H__
#define __USER_POWER_H__

#include "esp_common.h"

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


#endif
