/*
 * gas.c - Cảm biến khí gas MQ-2 (PA7 / ADC1_IN7) - THƯ VIỆN SPL (stm32f10x)
 * Tự khởi tạo GPIO, ADC1, DMA1 kênh 1 (Circular) và TIM4 làm bộ đếm thời gian.
 * Nối dây: VCC -> 5V, GND -> GND, AO -> 10k -> PA7, PA7 -> 20k -> GND
 */
#include "stm32f10x.h"
#include <math.h>

#define ADC_BUFFER_SIZE   100
#define RL                10000.0f         // điện trở tải module (5k hoặc 10k)
#define VCC               5.0f
#define R1                10000.0f         // AO -> R1 -> PA7
#define R2                20000.0f         // PA7 -> R2 -> GND
#define CLEAN_AIR_RATIO   9.83f
#define WARMUP_MS         180000UL         // 3 phút

static uint16_t adc_buffer[ADC_BUFFER_SIZE];

static float    R0 = 0;
static uint8_t  sensor_ready = 0;
static uint32_t start_time = 0;
static uint32_t last_update = 0;
static uint16_t gas_raw = 0;
static uint16_t gas_ppm = 0;

/* ---------- Bộ đếm thời gian (ms) bằng TIM4, không cần ngắt ---------- */
static uint16_t last_cnt = 0;
static uint32_t tick_acc = 0;            
static uint32_t ms_total = 0;

static void timebase_init(void)
{
    TIM_TimeBaseInitTypeDef t;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    t.TIM_Prescaler         = (uint16_t)(SystemCoreClock / 10000UL - 1);  
    t.TIM_Period            = 0xFFFF;
    t.TIM_ClockDivision     = TIM_CKD_DIV1;
    t.TIM_CounterMode       = TIM_CounterMode_Up;
    t.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &t);
    TIM_Cmd(TIM4, ENABLE);
}

static uint32_t millis(void)
{
    uint16_t cnt   = TIM_GetCounter(TIM4);
    uint16_t delta = (uint16_t)(cnt - last_cnt);   
    last_cnt = cnt;

    tick_acc += delta;
    ms_total += tick_acc / 10;
    tick_acc %= 10;
    return ms_total;
}

/* ---------- Khởi tạo phần cứng ---------- */
static void mq2_hw_init(void)
{
    GPIO_InitTypeDef  g;
    DMA_InitTypeDef   d;
    ADC_InitTypeDef   a;
    RCC_ClocksTypeDef clocks;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* PA7 = analog input */
    g.GPIO_Pin  = GPIO_Pin_7;
    g.GPIO_Mode = GPIO_Mode_AIN;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &g);

    /* Xung ADC phải <= 14 MHz: tự chọn bộ chia theo PCLK2 */
    RCC_GetClocksFreq(&clocks);
    if (clocks.PCLK2_Frequency <= 28000000UL)      RCC_ADCCLKConfig(RCC_PCLK2_Div2);
    else if (clocks.PCLK2_Frequency <= 56000000UL) RCC_ADCCLKConfig(RCC_PCLK2_Div4);
    else if (clocks.PCLK2_Frequency <= 84000000UL) RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    else                                           RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    /* DMA1 kênh 1: ADC1->DR sang adc_buffer, Circular, Half Word */
    DMA_DeInit(DMA1_Channel1);
    d.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    d.DMA_MemoryBaseAddr     = (uint32_t)adc_buffer;
    d.DMA_DIR                = DMA_DIR_PeripheralSRC;
    d.DMA_BufferSize         = ADC_BUFFER_SIZE;
    d.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    d.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    d.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    d.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    d.DMA_Mode               = DMA_Mode_Circular;
    d.DMA_Priority           = DMA_Priority_Medium;
    d.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &d);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* ADC1: kênh 7, chạy liên tục */
    a.ADC_Mode               = ADC_Mode_Independent;
    a.ADC_ScanConvMode       = DISABLE;
    a.ADC_ContinuousConvMode = ENABLE;
    a.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    a.ADC_DataAlign          = ADC_DataAlign_Right;
    a.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &a);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 1, ADC_SampleTime_71Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* Hiệu chuẩn ADC */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    /* Bắt đầu đọc liên tục */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/* ---------- Tính toán ---------- */
static uint16_t adc_average(void)
{
    uint32_t sum = 0;
    uint16_t i;
    for (i = 0; i < ADC_BUFFER_SIZE; i++) sum += adc_buffer[i];
    return (uint16_t)(sum / ADC_BUFFER_SIZE);
}
static float get_voltage(uint16_t adc)
{
    float v_adc = adc * 3.3f / 4095.0f;
    return v_adc * (R1 + R2) / R2;         
}
static float get_Rs(float vout)
{
    if (vout <= 0.01f || vout >= VCC) return 0;
    return RL * (VCC - vout) / vout;
}
static float get_ppm(float Rs)
{
    if (R0 <= 0 || Rs <= 0) return 0;
    return 574.25f * powf(Rs / R0, -2.222f);
}

/* ===== Các hàm main.c được gọi ===== */

void MQ2_Init(void)
{
    timebase_init();
    mq2_hw_init();
    start_time  = millis();
    last_update = start_time;
}

void MQ2_Process(void)                     
{
    uint32_t now = millis();
    float Rs, ppm;
    if (now - last_update < 1000) return;
    last_update = now;
    gas_raw = adc_average();
    Rs = get_Rs(get_voltage(gas_raw));
    if (!sensor_ready)
    {
        if (now - start_time >= WARMUP_MS && Rs > 0)
        {
            R0 = Rs / CLEAN_AIR_RATIO;    
            sensor_ready = 1;
        }
        gas_ppm = 0;
        return;
    }
    ppm = get_ppm(Rs);
    if (ppm > 65535.0f) ppm = 65535.0f;
    gas_ppm = (uint16_t)ppm;
}
uint16_t MQ2_GetPPM(void)   { return gas_ppm; }
uint16_t MQ2_GetRaw(void)   { return gas_raw; }
uint8_t  MQ2_IsReady(void)  { return sensor_ready; }
