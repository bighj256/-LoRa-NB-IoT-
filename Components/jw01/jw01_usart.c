#include "jw01_usart.h"

static TIM_TypeDef *g_tim_handle = JW01_TIM_INTERFACE;
static USART_TypeDef *g_uart_handle = JW01_UART_INTERFACE;

/* JW01 串口接收帧缓冲信息结构体 */
static struct
{
    uint8_t buf[JW01_UART_RX_BUF_SIZE]; /* 接收帧缓冲区 */
    struct
    {
        uint16_t len   : 15; /* 帧接收长度，sta[14:0] */
        uint16_t finsh : 1;  /* 帧接收完成标志，sta[15] */
    } sta;                   /* 帧状态信息 */
} g_uart_rx_frame = {0};    /* JW01 UART接收帧缓冲信息结构体 */

/**
 * @brief       重置 JW01 串口接收状态机
 * @param       无
 * @retval      无
 * @note        在开始读取下一帧新数据前，必须调用此函数将接收长度清零、完成标志复位
 */
void jw01_uart_rx_restart(void)
{
    g_uart_rx_frame.sta.len   = 0;
    g_uart_rx_frame.sta.finsh = 0;
}

/**
 * @brief       获取接收到的完整 JW01 数据帧
 * @param       无
 * @retval      NULL : 尚未接收到完整的数据帧
 *              其他 : 指向接收缓冲区的指针，已在末尾自动追加 '\0'
 * @note        通过判断帧接收完成标志 finsh 是否为 1 来决定是否返回数据
 */
uint8_t *jw01_uart_rx_get_frame(void)
{
    if (g_uart_rx_frame.sta.finsh == 1) {
        g_uart_rx_frame.buf[g_uart_rx_frame.sta.len] = '\0';
        return g_uart_rx_frame.buf;
    }
    return NULL;
}

/**
 * @brief       获取当前接收到的完整 JW01 数据帧的长度
 * @param       无
 * @retval      0    : 尚未接收到完整数据帧
 *              其他 : 完整数据帧的实际字节数
 */
uint16_t jw01_uart_rx_get_frame_len(void)
{
    if (g_uart_rx_frame.sta.finsh == 1)
        return g_uart_rx_frame.sta.len;
    return 0;
}

/**
 * @brief       JW01 超时定时器底层 MSP 初始化
 * @param       无
 * @retval      无
 * @note        主要负责使能定时器的硬件外设时钟，并配置定时器中断在 NVIC 中的通道与优先级
 */
static void jw01_timer_msp_init(void)
{
    JW01_TIM_CLK_ENABLE();

    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel                   = JW01_TIM_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority  = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

/**
 * @brief       JW01 超时定时器时基配置初始化
 * @param       无
 * @retval      无
 * @note        配置定时器预分频数和自动重装载值，使其产生 10ms 的更新（溢出）中断，并开启中断使能
 */
void jw01_timer_init(void)
{
    jw01_timer_msp_init();

    TIM_TimeBaseInitTypeDef tim;
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler     = JW01_TIM_PRESCALER - 1;
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_Period        = 100 - 1;          /* 10ms  */
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(g_tim_handle, &tim);

    TIM_ITConfig(g_tim_handle, TIM_IT_Update, ENABLE);
}

/**
 * @brief       JW01 传感器专用串口及相关引脚初始化
 * @param       baudrate : 串口通信波特率（如 9600）
 * @retval      无
 * @note        步骤包括：使能串口与GPIO时钟、配置TX/RX引脚模式、配置串口通信参数（8N1）、
 *              开启接收中断、配置中断通道优先级，并最终初始化超时断帧定时器
 */
void jw01_uart_init(uint32_t baudrate)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;

    /* 使能时钟 */
    JW01_UART_CLK_ENABLE();
    JW01_UART_TX_GPIO_CLK_ENABLE();
    JW01_UART_RX_GPIO_CLK_ENABLE();

    /* TX 引脚：复用推挽输出 */
    gpio.GPIO_Pin   = JW01_UART_TX_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(JW01_UART_TX_GPIO_PORT, &gpio);

    /* RX 引脚：浮空输入 */
    gpio.GPIO_Pin   = JW01_UART_RX_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(JW01_UART_RX_GPIO_PORT, &gpio);

    /* USART 配置 */
    USART_StructInit(&usart);
    usart.USART_BaudRate            = baudrate;                         //波特率
    usart.USART_WordLength          = USART_WordLength_8b;              //8位数据位
    usart.USART_StopBits            = USART_StopBits_1;                 //1位停止位
    usart.USART_Parity              = USART_Parity_No;                  //无校验位
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;    //收发模式
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;   //不使用硬件流控
    USART_Init(g_uart_handle, &usart);

    /* 使能 RXNE 中断 当RXNE不为空时 触发中断 */
    USART_ITConfig(g_uart_handle, USART_IT_RXNE, ENABLE);

    /* 中断优先级 */
    nvic.NVIC_IRQChannel                   = JW01_UART_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;   /* 可根据系统调整 */
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(g_uart_handle, ENABLE);

    jw01_timer_init();
}

/**
 * @brief       JW01 传感器串口中断服务函数
 * @param       无
 * @retval      无
 * @note        负责接收每个传入字节。具备 ORE（溢出错误）清除保护机制防止死锁。
 *              每收到一个字节重置定时器，如果为首字节则开启定时器计时
 */
void JW01_UART_IRQHandler(void)
{
    uint8_t tmp;
    
    /* 判断溢出错误 ORE，防止死锁 */
    if (USART_GetFlagStatus(g_uart_handle, USART_FLAG_ORE) != RESET) {
        USART_ClearFlag(g_uart_handle, USART_FLAG_ORE);
        (void)USART_ReceiveData(g_uart_handle);
    }

    /* 判断接收缓冲区是否非空 */
    if (USART_GetITStatus(g_uart_handle, USART_IT_RXNE) != RESET) {
        tmp = USART_ReceiveData(g_uart_handle);

        /* 判断接收缓冲区是否已满 */
        if (g_uart_rx_frame.sta.len < JW01_UART_RX_BUF_SIZE - 1) {
            /* 接收到数据，重置定时器 */
            TIM_SetCounter(g_tim_handle, 0);
            /* 如果是首字节则开启定时器 */
            if (g_uart_rx_frame.sta.len == 0)
                TIM_Cmd(g_tim_handle, ENABLE);
            g_uart_rx_frame.buf[g_uart_rx_frame.sta.len] = tmp;
            g_uart_rx_frame.sta.len++;
        } else {
            /* 缓冲区溢出，重置 */
            g_uart_rx_frame.sta.len = 0;
            g_uart_rx_frame.buf[g_uart_rx_frame.sta.len] = tmp;
            g_uart_rx_frame.sta.len++;
        }
        USART_ClearITPendingBit(g_uart_handle, USART_IT_RXNE);//清除串口的“接收缓冲区非空（RXNE）”中断挂起（Pending）标志位
    }
}

/**
 * @brief       JW01 超时断帧定时器中断服务函数
 * @param       无
 * @retval      无
 * @note        当串口停止传输字符超过 10ms 时触发此中断。
 *              负责清除中断挂起标志、置起帧接收完成标志 finsh 并关闭定时器以等待下一帧
 */
void JW01_TIM_IRQHandler(void)
{
    if (TIM_GetITStatus(g_tim_handle, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(g_tim_handle, TIM_IT_Update);//清定时器的“更新(Update)”中断挂起标志
        g_uart_rx_frame.sta.finsh = 1;//设置帧接收完成标志
        TIM_Cmd(g_tim_handle, DISABLE);//关闭定时器
    }
}
