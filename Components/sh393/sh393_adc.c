#include "sh393_adc.h"

static ADC_TypeDef *g_adc_handle = SH393_ADC_INTERFACE;

/**
 * @brief       SH393 ADC 初始化（GPIO、时钟、校准，带超时保护）
 * @param       无
 * @retval      SH393_ADC_EOK   : 初始化成功
 *              SH393_ADC_ERROR : 初始化失败
 */
uint8_t sh393_adc_init(void)
{
    GPIO_InitTypeDef gpio;

    /* ─── 1. GPIO 初始化（模拟输入）─── */
    SH393_ADC_GPIO_CLK_ENABLE();
    gpio.GPIO_Pin   = SH393_ADC_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AIN;            //在AIN模式下 GPIO无效 断开GPIO 防止GPIO口输入输出对模拟电压造成干扰
    GPIO_Init(SH393_ADC_GPIO_PORT, &gpio);

        /* 1. 初始化 ADC 外设（如果尚未初始化） */
    if (adcx_init(g_adc_handle) != ADC_EOK)   // SH393_ADC_INTERFACE 定义为 ADC1
        return SH393_ADC_ERROR;

    return SH393_ADC_EOK;
}

/**
 * @brief       读取 SH393 ADC 原始值（中值滤波）
 * @param       value: 存放 ADC 转换结果的指针
 * @retval      SH393_ADC_EOK   : 读取成功
 *              SH393_ADC_ERROR : 读取失败
 */
uint8_t sh393_adc_read(uint16_t *value)
{
    return adcx_get_value_filter(g_adc_handle, SH393_ADC_CHANNEL, ADC_SampleTime_239Cycles5, 8, value);
}
