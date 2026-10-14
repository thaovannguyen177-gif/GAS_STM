/*
 * led.c - LED đơn ở PA0 - THƯ VIỆN SPL (stm32f10x)
 * Nối dây: PA0 -> 220R -> LED(+), LED(-) -> GND  (sáng khi PA0 mức cao)
 */
#include "stm32f10x.h"

#define LED_PORT  GPIOA
#define LED_PIN   GPIO_Pin_0

static uint8_t led_state = 1;

void LED_Set(uint8_t state)                // 1 = bật, 0 = tắt
{
    led_state = state ? 1 : 0;
    if (led_state) GPIO_SetBits(LED_PORT, LED_PIN);
    else           GPIO_ResetBits(LED_PORT, LED_PIN);
}

void LED_Init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    g.GPIO_Pin   = LED_PIN;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LED_PORT, &g);

    LED_Set(1);                            // mặc định bật
}

uint8_t LED_Get(void)                      // trạng thái hiện tại
{
    return led_state;
}
