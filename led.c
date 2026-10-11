#include "led.h"

static uint8_t led_state = 1;

void LED_Set(uint8_t state)
{
    led_state = state ? 1 : 0;
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LED_Toggle(void)
{
    LED_Set(!led_state);
}

void LED_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin   = LED_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &g);

    LED_Set(1);                    // mặc định bật
}

uint8_t LED_Get(void)
{
    return led_state;
}
