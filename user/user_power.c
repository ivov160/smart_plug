#include "user_power.h"

#include "gpio.h"

#define POWER_GPIO 12
#define POWER_GPIO_MUX PERIPHS_IO_MUX_MTDI_U
#define POWER_GPIO_FUNC FUNC_GPIO12

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
