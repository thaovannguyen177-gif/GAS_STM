#ifndef MQ2_H
#define MQ2_H

#include "main.h"

/* ---- Cấu hình phần cứng ---- */
#define MQ2_ADC_BUFFER_SIZE   100
#define MQ2_RL                10000.0f   // điện trở tải trên module MQ-2 (kiểm tra thực tế: 5k hoặc 10k)
#define MQ2_VCC               5.0f       // nguồn cấp cho MQ-2
#define MQ2_R1                10000.0f   // cầu chia áp: AO -> R1 -> PA7
#define MQ2_R2                20000.0f   //              PA7 -> R2 -> GND
#define MQ2_CLEAN_AIR_RATIO   9.83f      // Rs/R0 trong không khí sạch (datasheet MQ-2)

/* Thời gian làm nóng trước khi hiệu chuẩn R0 (ms).
 * Demo: 180000 (3 phút). Khi test nhanh có thể hạ xuống 5000. */
#define MQ2_WARMUP_MS         180000UL

/* Ngưỡng cảnh báo rò rỉ gas (ppm) */
#define MQ2_ALARM_PPM         1000

void     MQ2_Init(void);          // gọi 1 lần sau MX_ADC1_Init() và MX_DMA_Init()
void     MQ2_Process(void);       // gọi liên tục trong while(1)

uint8_t  MQ2_IsReady(void);       // 1 = đã warm-up xong và đã có R0
uint16_t MQ2_GetPPM(void);        // nồng độ gas (ppm), 0 nếu chưa sẵn sàng
uint16_t MQ2_GetRaw(void);        // ADC trung bình 0..4095
float    MQ2_GetVoltage(void);    // điện áp AO thực (V)
float    MQ2_GetR0(void);         // R0 đã hiệu chuẩn (ohm)
uint8_t  MQ2_IsAlarm(void);       // 1 = vượt ngưỡng

#endif
