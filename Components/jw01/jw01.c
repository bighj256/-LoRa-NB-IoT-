#include "jw01.h"

/**
 * @brief  JW01 传感器初始化
 * @param  baudrate: 串口波特率 (通常 9600)
 * @retval JW01_EOK 成功
 */
uint8_t jw01_init(void)
{
    uint16_t dummy_ppm;

    /* 1. 初始化串口波特率 (通常为 9600) */
    jw01_uart_init(9600);

    /* 2. 尝试在最长 1.5 秒内获取一次有效数据帧进行自检握手 */
    uint32_t retry = 15;
    while (retry > 0)
    {
        if (jw01_measure(&dummy_ppm) == JW01_EOK)
        {
            return JW01_EOK;   /* 成功收到有效数据帧，自检通过 */
        }
        delay_ms(100);
        retry--;
    }

    return JW01_ERR_TIMEOUT;   /* 超时未收到正常数据包，自检失败 */
}

/**
 * @brief  读取二氧化碳浓度
 * @param  ppm: 输出浓度值 (ppm)
 * @retval JW01_EOK        成功
 *         JW01_ERR_CHECKSUM 校验错误
 *         JW01_ERR_TIMEOUT  超时未收到数据
 */
uint8_t jw01_measure(uint16_t *ppm)
{
    uint8_t *frame = NULL;
    uint32_t timeout = 1000; // 等待 1 秒

    /* 在开始接收前复位接收状态，清除旧缓存与垃圾字节 */
    jw01_uart_rx_restart();

    /* 等待一个新帧 */
    while (timeout > 0) {
        frame = jw01_uart_rx_get_frame();
        if (frame != NULL) {
            break;
        }
        timeout--;
        delay_ms(1);
    }

    if (frame == NULL) {
        return JW01_ERR_TIMEOUT;
    }

    /* 校验数据包格式：长度6，并且首字节必须为帧头 0x2C */
    uint16_t len = jw01_uart_rx_get_frame_len();
    if (len < JW01_PACKET_LEN || frame[0] != 0x2C) {
        return JW01_ERR_INVALID;
    }

    /* 校验和：前5字节相加的低字节等于第6字节 */
    uint8_t sum = frame[0] + frame[1] + frame[2] + frame[3] + frame[4];
    if (sum != frame[5]) {
        return JW01_ERR_CHECKSUM;
    }

    /* 提取浓度值（高字节在前） */
    *ppm = (uint16_t)(frame[1] << 8) | frame[2];
    return JW01_EOK;
}
