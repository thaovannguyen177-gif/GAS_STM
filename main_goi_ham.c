/*
 * main.c mẫu cho project SPL: gọi gas.c và led.c (không có file .h)
 * Vì không có .h nên main.c tự khai báo các hàm ở đầu file.
 * Nếu đã có main.c của cả nhóm, chỉ cần thêm phần khai báo + 3 chỗ gọi hàm.
 */
#include "stm32f10x.h"

/* Hàm trong gas.c */
void     MQ2_Init(void);
void     MQ2_Process(void);
uint16_t MQ2_GetPPM(void);
uint16_t MQ2_GetRaw(void);
uint8_t  MQ2_IsReady(void);

/* Hàm trong led.c */
void     LED_Init(void);
void     LED_Set(uint8_t state);
uint8_t  LED_Get(void);

int main(void)
{
    LED_Init();
    MQ2_Init();

    while (1)
    {
        MQ2_Process();      // gọi liên tục, bên trong tự cập nhật mỗi 1 giây

        /* Người làm UART dùng:
         *   MQ2_GetPPM()  -> gửi trường "G:"
         *   LED_Get()     -> gửi trường "L:"
         *   LED_Set(1/0)  -> khi nhận lệnh "LED:1" / "LED:0"
         */
    }
}
