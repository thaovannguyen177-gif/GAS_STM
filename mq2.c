#include "mq2.h"
#include <math.h>

extern ADC_HandleTypeDef hadc1;

static uint16_t adc_buffer[MQ2_ADC_BUFFER_SIZE];   // DMA ghi liên tục (Circular)

static float    R0          = 0.0f;
static uint8_t  sensor_ready = 0;
static uint32_t start_time  = 0;
static uint32_t last_update = 0;

static uint16_t g_raw     = 0;
static float    g_voltage = 0.0f;
static uint16_t g_ppm     = 0;

/* ---------- Hàm nội bộ ---------- */

static uint16_t adc_average(void)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < MQ2_ADC_BUFFER_SIZE; i++)
        sum += adc_buffer[i];
    return (uint16_t)(sum / MQ2_ADC_BUFFER_SIZE);
}

/* ADC -> điện áp thực tại chân AO (đã bù cầu chia áp) */
static float get_voltage(uint16_t adc)
{
    float v_adc = adc * 3.3f / 4095.0f;
    return v_adc * (MQ2_R1 + MQ2_R2) / MQ2_R2;
}

/* Điện áp AO -> điện trở cảm biến Rs */
static float get_Rs(float vout)
{
    if (vout <= 0.01f || vout >= MQ2_VCC) return 0.0f;
    return MQ2_RL * (MQ2_VCC - vout) / vout;
}

/* Rs -> ppm (đường cong LPG của MQ-2) */
static float get_ppm(float Rs)
{
    if (R0 <= 0.0f || Rs <= 0.0f) return 0.0f;
    float ratio = Rs / R0;
    return 574.25f * powf(ratio, -2.222f);
}

/* ---------- Hàm công khai ---------- */

void MQ2_Init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, MQ2_ADC_BUFFER_SIZE);
    start_time  = HAL_GetTick();
    last_update = start_time;
}

void MQ2_Process(void)
{
    uint32_t now = HAL_GetTick();

    /* Cập nhật mỗi 1 giây */
    if (now - last_update < 1000) return;
    last_update = now;

    g_raw     = adc_average();
    g_voltage = get_voltage(g_raw);
    float Rs  = get_Rs(g_voltage);

    /* Giai đoạn warm-up: chưa tính ppm */
    if (!sensor_ready)
    {
        if (now - start_time >= MQ2_WARMUP_MS && Rs > 0.0f)
        {
            R0 = Rs / MQ2_CLEAN_AIR_RATIO;   // hiệu chuẩn trong không khí sạch
            sensor_ready = 1;
        }
        g_ppm = 0;
        return;
    }

    float ppm = get_ppm(Rs);
    if (ppm > 65535.0f) ppm = 65535.0f;
    g_ppm = (uint16_t)ppm;
}

uint8_t  MQ2_IsReady(void)    { return sensor_ready; }
uint16_t MQ2_GetPPM(void)     { return g_ppm; }
uint16_t MQ2_GetRaw(void)     { return g_raw; }
float    MQ2_GetVoltage(void) { return g_voltage; }
float    MQ2_GetR0(void)      { return R0; }
uint8_t  MQ2_IsAlarm(void)    { return (sensor_ready && g_ppm > MQ2_ALARM_PPM) ? 1 : 0; }
