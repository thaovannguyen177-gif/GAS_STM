/*Cam bien khi GAS*/
#include "stm32f10x.h"
#include <math.h>

#define RL                10000.0f     // điện trở tải module (5k hoặc 10k, kiểm tra thực tế)
#define VCC               5.0f
#define CLEAN_AIR_RATIO   9.83f        // Rs/R0 trong không khí sạch (datasheet)

static float R0 = 0;                   // 0 = chưa hiệu chuẩn

/* ---------- Khởi tạo ---------- */
void MQ2_Init(void)
{
    GPIO_InitTypeDef g;
    ADC_InitTypeDef  a;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);            // 72 MHz / 6 = 12 MHz (<= 14 MHz)

    g.GPIO_Pin   = GPIO_Pin_7;                   // PA7 = ngõ vào analog
    g.GPIO_Mode  = GPIO_Mode_AIN;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &g);

    a.ADC_Mode               = ADC_Mode_Independent;
    a.ADC_ScanConvMode       = DISABLE;
    a.ADC_ContinuousConvMode = DISABLE;          // mỗi lần gọi đọc 1 mẫu
    a.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    a.ADC_DataAlign          = ADC_DataAlign_Right;
    a.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &a);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 1, ADC_SampleTime_71Cycles5);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);                  // hiệu chuẩn ADC
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/* ---------- Delay chờ bằng SysTick (polling, không dùng ngắt) ---------- */
void MQ2_DelayMs(uint32_t ms)
{
    SysTick->LOAD = SystemCoreClock / 1000 - 1;  // 1 ms
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;                           // clock = HCLK, bật, không ngắt
    while (ms--)
    {
        while (!(SysTick->CTRL & (1UL << 16)));  // chờ cờ COUNTFLAG (đủ 1 ms)
    }
    SysTick->CTRL = 0;                           // tắt SysTick
}

/* ---------- Đọc ADC ---------- */
uint16_t MQ2_ReadRaw(void)                       // 1 mẫu, 0..4095
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);
}
uint16_t MQ2_Read(void)                          // trung bình 10 mẫu
{
    uint32_t sum = 0;
    uint8_t i;
    for (i = 0; i < 10; i++) sum += MQ2_ReadRaw();
    return (uint16_t)(sum / 10);
}

/* Điện áp thật tại đầu AO (V), đã bù cầu chia áp 10k/10k (hệ số 2) */
static float get_voltage(void)
{
    return MQ2_Read() * 3.3f / 4095.0f * 2f;
}
uint16_t MQ2_ReadMilliVolt(void)
{
    return (uint16_t)(get_voltage() * 1000.0f);
}

/* ---------- Điện trở cảm biến Rs ---------- */
static float get_Rs(void)
{
    float v = get_voltage();
    if (v <= 0.01f || v >= VCC) return 0;
    return RL * (VCC - v) / v;
}

/* ---------- Hiệu chuẩn R0 (gọi khi đang ở không khí sạch) ---------- */
void MQ2_Calibrate(void)
{
    float sum = 0;
    uint8_t i;
    for (i = 0; i < 10; i++)
    {
        sum += get_Rs();
        MQ2_DelayMs(100);
    }
    R0 = (sum / 10.0f) / CLEAN_AIR_RATIO;
}
uint8_t MQ2_IsCalibrated(void)
{
    return (R0 > 0) ? 1 : 0;
}

/* ---------- Đọc ppm (trả về 0 nếu chưa hiệu chuẩn) ---------- */
uint16_t MQ2_ReadPPM(void)
{
    float rs = get_Rs();
    float ppm;

    if (R0 <= 0 || rs <= 0) return 0;
    ppm = 574.25f * powf(rs / R0, -2.222f);
    if (ppm > 65535.0f) ppm = 65535.0f;
    return (uint16_t)ppm;
}
