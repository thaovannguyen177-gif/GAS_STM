#ifndef LED_H
#define LED_H

#include "main.h"

#define LED_PORT  GPIOA
#define LED_PIN   GPIO_PIN_0       // LED ngoài, sáng khi PA0 ở mức cao

void    LED_Init(void);            // gọi 1 lần sau MX_GPIO_Init()
void    LED_Set(uint8_t state);    // 1 = bật, 0 = tắt
void    LED_Toggle(void);
uint8_t LED_Get(void);             // trạng thái hiện tại

#endif
