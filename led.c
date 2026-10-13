/*
 * led.c - LED đơn ở PA0 (bản không dùng static)
 * Nối dây: PA0 -> 220R -> LED(+), LED(-) -> GND  (sáng khi PA0 mức cao)
 */
#include "stm32f1xx_hal.h"

#define LED_PORT  GPIOA
#define LED_PIN   GPIO_PIN_0

uint8_t led_state = 1;                     // biến toàn cục: 1 = bật, 0 = tắt

void LED_Set(uint8_t state)                // 1 = bật, 0 = tắt
{
    led_state = state ? 1 : 0;
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
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
    LED_Set(1);                            // mặc định bật
}

uint8_t LED_Get(void)                      // trạng thái hiện tại
{
    return led_state;
}
