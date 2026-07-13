#include "adc.h"


static uint8_t first_init = 1;

uint8_t adcx_init(ADC_TypeDef* ADCx)
{
    ADC_InitTypeDef adc;
    uint32_t timeout;

    if(!first_init) 
    {
        first_init = 0;
        return ADC_EOK;
    }

    /* 1. 参数检查 */
    if (ADCx == NULL) return ADC_ERROR;
    if (ADCx != ADC1 && ADCx != ADC2 && ADCx != ADC3) return ADC_ERROR;  // 可选

    /* 2. 根据 ADCx 使能对应时钟 */
    if (ADCx == ADC1) RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    else if (ADCx == ADC2) RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    else if (ADCx == ADC3) RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);
    else return ADC_ERROR;

    /* 3. 配置 ADC 时钟分频（全局配置一次即可，多次调用无害） */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 4. ADC 基础配置 单次转换非扫描模式*/
    adc.ADC_Mode               = ADC_Mode_Independent;         //ADC独立模式
    adc.ADC_DataAlign          = ADC_DataAlign_Right;          //右对齐
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;    //不使用外部触发 使用软件触发
    adc.ADC_ContinuousConvMode = DISABLE;                      //单次转换模式
    adc.ADC_ScanConvMode       = DISABLE;                      //扫描模式关闭
    adc.ADC_NbrOfChannel       = 1;                            //该模式下ADC通道只能有1个
    ADC_Init(ADCx, &adc);

    /* 5. 使能 ADC 并校准 */
    ADC_Cmd(ADCx, ENABLE);
    delay_us(20);   // 等待稳定

    /*ADC复位校准*/
    ADC_ResetCalibration(ADCx);
    timeout = ADC_CALIB_TIMEOUT;
    /*等待复位校准完成*/
    while (ADC_GetResetCalibrationStatus(ADCx) == SET) {
        if (--timeout == 0) return ADC_ERROR;
        delay_us(10);
    }
    /*ADC开始校准*/
    ADC_StartCalibration(ADCx);
    timeout = ADC_CALIB_TIMEOUT;
    /*等待开始校准完成*/
    while (ADC_GetCalibrationStatus(ADCx) == SET) {
        if (--timeout == 0) return ADC_ERROR;
        delay_us(10);
    }

    return ADC_EOK;
}

/**
 * @brief  ADC单次采集（带超时保护）
 * @param  ADCx           ADC外设，如ADC1
 * @param  ADC_Channel    通道号
 * @param  ADC_SampleTime 采样周期，pH推荐239.5周期
 * @param  value          出参，存放12位转换结果
 * @retval ADC_EOK        成功
 * @retval ADC_ERROR      超时或空指针
 */
uint8_t adcx_get_value(ADC_TypeDef* ADCx, uint8_t ADC_Channel, uint8_t ADC_SampleTime, uint16_t *value)
{
    uint32_t timeout;

    if (value == NULL) return ADC_ERROR;

    /*ADC规则通道转换*/
    ADC_RegularChannelConfig(ADCx, ADC_Channel, 1, ADC_SampleTime);
    /*ADC软件触发转换*/
    ADC_SoftwareStartConvCmd(ADCx, ENABLE);

    timeout = ADC_SAMPLE_TIMEOUT;
    /*等待EOC标志*/
    while (ADC_GetFlagStatus(ADCx, ADC_FLAG_EOC) == RESET)
    {
        if (--timeout == 0)     // 超时退出，防止死锁
        {
            *value = 0;
            return ADC_ERROR;
        }
        delay_us(10);
    }
    /*获取转换值*/
    *value = ADC_GetConversionValue(ADCx);  // 自动清除EOC标志
    return ADC_EOK;
}

/**
 * @brief  ADC多次采集并中值平均滤波（带超时保护）
 * @note   滤波逻辑会自动剔除排序后的一个最大值和一个最小值，再对剩余的 (n-2) 个样本取平均
 * @param  ADCx           ADC外设，如ADC1
 * @param  ADC_Channel    通道号
 * @param  ADC_SampleTime 采样周期，精密模拟量传感器推荐239.5周期
 * @param  n              采样次数，有效范围为 4 ~ 16 次
 * @param  value          出参，存放12位滤波后的转换结果
 * @retval ADC_EOK        成功
 * @retval ADC_ERROR      超时、采样次数越界或空指针
 */
uint8_t adcx_get_value_filter(ADC_TypeDef* ADCx, uint8_t ADC_Channel, uint8_t ADC_SampleTime, uint8_t n, uint16_t *value)
{
    // 中值平均滤波：采集n次，排序去极值（防突发电磁脉冲干扰），中间项取平均（平滑高频随机噪声）
    uint16_t buf[16];
    uint16_t tmp;
    uint8_t  round, cmp;

    if (value == NULL || n > 16 || n < 4) return ADC_ERROR;

    /* 连续采集n次 */
    for (round = 0; round < n; round++)
    {
        uint8_t ret = adcx_get_value(ADCx, ADC_Channel, ADC_SampleTime, &buf[round]);
        if (ret != ADC_EOK) return ADC_ERROR;
    }

    /* 排序 */
    for (round = 0; round < n - 1; round++)
    {
        for (cmp = round + 1; cmp < n; cmp++)
        {
            if (buf[round] > buf[cmp])
            {
                tmp        = buf[round];
                buf[round] = buf[cmp];
                buf[cmp]  = tmp;
            }
        }
    }

    /* 动态去掉头尾两个极值，中间 n-2 个取平均 */
    uint32_t sum = 0;
    for (round = 1; round < n - 1; round++)
    {
        sum += buf[round];
    }
    *value = sum / (n - 2);

    return ADC_EOK;
}
