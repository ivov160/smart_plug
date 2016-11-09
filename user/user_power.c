#include "user_power.h"

#include "gpio.h"

/**
 * @defgroup user User 
 * @defgroup user_power User power
 *
 * @addtogroup user
 * @{
 * @addtogroup user_power 
 * @{
 */

/** 
 * @brief Нога для управлением нагрузкой plug
 */
#define POWER_GPIO 14

/**
 * @brief Регистр для настройки используемого вывода
 */
#define POWER_GPIO_MUX PERIPHS_IO_MUX_MTMS_U

/**
 * @brief Используемая функция указанного вывода
 */
#define POWER_GPIO_FUNC FUNC_GPIO14

static os_timer_t test_mode_timer;
static uint8_t power_state = 0;

static void power_test_mode_callback(void *p_args)
{
	GPIO_OUTPUT_SET(POWER_GPIO, power_state);
	power_state ^= 1;
}

void power_init()
{
	PIN_FUNC_SELECT(POWER_GPIO_MUX, POWER_GPIO_FUNC);
}

void power_down()
{
	GPIO_OUTPUT_SET(POWER_GPIO, 0);
}

void power_up()
{
	GPIO_OUTPUT_SET(POWER_GPIO, 1);
}

int8_t power_status()
{
	return GPIO_INPUT_GET(POWER_GPIO);
}


void power_start_test_mode(uint32_t ms)
{
	power_stop_test_mode();
	os_timer_setfn(&test_mode_timer, power_test_mode_callback, NULL);
	os_timer_arm(&test_mode_timer, ms, true);
}

void power_stop_test_mode()
{
	os_timer_disarm(&test_mode_timer);
	power_down();
}

/**
 * @}
 * @}
 */
