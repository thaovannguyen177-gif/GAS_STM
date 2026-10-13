/*
 * gas.c - MQ-2 (PA7 / ADC1_IN7) - TỰ KHỞI TẠO ADC + DMA, không phụ thuộc CubeMX
 * Chỉ cần thêm file này vào project Keil, rồi gọi MQ2_Init() và MQ2_Process() trong main.c
 *
 * Nối dây: VCC -> 5V, GND -> GND, AO -> 10k -> PA7, PA7 -> 20k -> GND
 *
 * Yêu cầu project: đã bật HAL_ADC_MODULE_ENABLED, HAL_DMA_MODULE_ENABLED trong
 * stm32f1xx_hal_conf.h và có stm32f1xx_hal_adc.c, stm32f1xx_hal_adc_ex.c, stm32f1xx_hal_dma.c
 * Lưu ý: dùng cho project CHƯA cấu hình ADC1/DMA1 kênh 1 trong CubeMX (tránh khởi tạo 2 lần).
 */
#include "stm32f1xx_hal.h"
#include <math.h>

#define ADC_BUFFER_SIZE   100
#define RL                10000.0f         // điện trở tải module (5k hoặc 10k)
#define VCC               5.0f
#define R1                10000.0f         // AO -> R1 -> PA7
#define R2                20000.0f         // PA7 -> R2 -> GND
#define CLEAN_AIR_RATIO   9.83f
#define WARMUP_MS         180000UL         // 3 phút (test nhanh: 5000)

static ADC_HandleTypeDef mq2_adc;
static DMA_HandleTypeDef mq2_dma;
static uint16_t adc_buffer[ADC_BUFFER_SIZE];

static float    R0 = 0;
static uint8_t  sensor_ready = 0;
static uint32_t start_time = 0;
static uint32_t last_update = 0;
static uint16_t gas_raw = 0;
static uint16_t gas_ppm = 0;

/* ---------- Khởi tạo phần cứng ---------- */
static void mq2_hw_init(void)
{
    GPIO_InitTypeDef g = {0};
    ADC_ChannelConfTypeDef ch = {0};
    RCC_PeriphCLKInitTypeDef clk = {0};
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* PA7 = analog */
    g.Pin  = GPIO_PIN_7;
    g.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &g);

    /* Xung ADC phải <= 14 MHz: tự chọn bộ chia theo PCLK2 */
    clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    if (pclk2 <= 28000000UL)      clk.AdcClockSelection = RCC_ADCPCLK2_DIV2;
    else if (pclk2 <= 56000000UL) clk.AdcClockSelection = RCC_ADCPCLK2_DIV4;
    else                          clk.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    HAL_RCCEx_PeriphCLKConfig(&clk);

    /* DMA1 kênh 1: Circular, Half Word */
    mq2_dma.Instance                 = DMA1_Channel1;
    mq2_dma.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    mq2_dma.Init.PeriphInc           = DMA_PINC_DISABLE;
    mq2_dma.Init.MemInc              = DMA_MINC_ENABLE;
    mq2_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    mq2_dma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    mq2_dma.Init.Mode                = DMA_CIRCULAR;
    mq2_dma.Init.Priority            = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&mq2_dma);

    /* ADC1: kênh 7, chạy liên tục */
    mq2_adc.Instance                   = ADC1;
    mq2_adc.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    mq2_adc.Init.ContinuousConvMode    = ENABLE;
    mq2_adc.Init.DiscontinuousConvMode = DISABLE;
    mq2_adc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    mq2_adc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    mq2_adc.Init.NbrOfConversion       = 1;
    __HAL_LINKDMA(&mq2_adc, DMA_Handle, mq2_dma);
    HAL_ADC_Init(&mq2_adc);

    ch.Channel      = ADC_CHANNEL_7;
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&mq2_adc, &ch);

    HAL_ADCEx_Calibration_Start(&mq2_adc);                  // hiệu chuẩn ADC
    HAL_ADC_Start_DMA(&mq2_adc, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE);
}

/* ---------- Tính toán ---------- */
static uint16_t adc_average(void)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < ADC_BUFFER_SIZE; i++) sum += adc_buffer[i];
    return (uint16_t)(sum / ADC_BUFFER_SIZE);
}

static float get_voltage(uint16_t adc)
{
    float v_adc = adc * 3.3f / 4095.0f;
    return v_adc * (R1 + R2) / R2;         // bù cầu chia áp
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
    mq2_hw_init();
    start_time = HAL_GetTick();
    last_update = start_time;
}

void MQ2_Process(void)                     // gọi liên tục trong while(1)
{
    uint32_t now = HAL_GetTick();
    if (now - last_update < 1000) return;
    last_update = now;

    gas_raw  = adc_average();
    float Rs = get_Rs(get_voltage(gas_raw));

    if (!sensor_ready)
    {
        if (now - start_time >= WARMUP_MS && Rs > 0)
        {
            R0 = Rs / CLEAN_AIR_RATIO;     // hiệu chuẩn lúc không khí sạch
            sensor_ready = 1;
        }
        gas_ppm = 0;
        return;
    }

    float ppm = get_ppm(Rs);
    if (ppm > 65535.0f) ppm = 65535.0f;
    gas_ppm = (uint16_t)ppm;
}

uint16_t MQ2_GetPPM(void)   { return gas_ppm; }
uint16_t MQ2_GetRaw(void)   { return gas_raw; }
uint8_t  MQ2_IsReady(void)  { return sensor_ready; }
