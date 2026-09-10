#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include <stdarg.h>


	uint8_t Serial1_RxData;		//定义串口接收的数据变量
  uint8_t Serial1_RxFlag;		//定义串口接收的标志位变量
void Serial1_Init(void)
{
    /*开启时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);    // USART1时钟在APB2上
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);     // GPIOA时钟

    /*GPIO初始化*/
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // PA9作为USART1_TX（复用推挽输出）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;                 // PA9
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10作为USART1_RX（浮空输入）
    // 上拉输入，不是浮空。对端（蓝牙模块 / 树莓派）没上电或者没插的时候，
    // 悬空的 RX 脚被电机和驱动器的干扰一耦合就能合成一个起始位，HandleCmd
    // 拿到一个任意字节照执行 —— 'U' 是前进，'P' 是开始画车位。
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;             // 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;                // PA10
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*USART初始化*/
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART_InitStructure);                 // USART1

    /*中断配置*/
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);            // USART1接收中断

    /*NVIC配置*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;         // USART1中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    /*USART使能*/
    USART_Cmd(USART1, ENABLE);                                // USART1
}
void Serial1_SendByte(uint8_t Byte)
{
    USART_SendData(USART1, Byte);        // 改为USART1
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);  // 改为USART1
}

uint32_t Serial1_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y --)
    {
        Result *= X;
    }
    return Result;
}

void Serial1_SendNumber(uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i ++)
    {
        Serial1_SendByte(Number / Serial1_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/**
  * 函    数：取一个已收到的字节（标志位和数据一起读，原子）
  * 参    数：out 用来带回数据
  * 返 回 值：1 = 取到了，0 = 没有新字节
  */
uint8_t Serial1_TakeRxData(uint8_t *out)
{
    uint8_t got = 0;
    // 原来那种 GetRxFlag() + GetRxData() 是两次调用，中间可以被打断：标志位还是
    // 1 但数据已经被后一个字节盖掉了 —— 前一个字节静默消失；反过来标志位还没
    // 置上就先读了数据，同一个字节会被执行两次（'P' 画两次车位）。
    // 两边都圈在临界区里，读到的数据和清标志一定配套。
    __disable_irq();
    if (Serial1_RxFlag) {
        *out = Serial1_RxData;
        Serial1_RxFlag = 0;
        got = 1;
    }
    __enable_irq();
    return got;
}
void USART1_IRQHandler(void)  // 改为USART1中断处理函数
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)  // USART1
    {
        Serial1_RxData = USART_ReceiveData(USART1);         //USART1
        Serial1_RxFlag = 1;
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);    // USART1
    }
}


 uint8_t Serial2_RxData;		//定义串口接收的数据变量
 uint8_t Serial2_RxFlag;		//定义串口接收的标志位变量

/**
  * 函    数：串口初始化
  * 参    数：无
  * 返 回 值：无
  */
void Serial2_Init(void)
{
    /*开启时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);     

    /*GPIO初始化*/
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // PA2作为USART2_TX（复用推挽输出）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;                 // PA2
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA3作为USART2_RX（浮空输入）
    // 上拉输入，不是浮空。对端（蓝牙模块 / 树莓派）没上电或者没插的时候，
    // 悬空的 RX 脚被电机和驱动器的干扰一耦合就能合成一个起始位，HandleCmd
    // 拿到一个任意字节照执行 —— 'U' 是前进，'P' 是开始画车位。
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;             // 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;                 // PA3
		//GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*USART初始化*/
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &USART_InitStructure);                 // USART2

    /*中断配置*/
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);            //USART2

    /*NVIC配置*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;         // USART2中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    /*USART使能*/
    USART_Cmd(USART2, ENABLE);                                // USART2
}
/**
  * 函    数：串口发送一个字节
  * 参    数：Byte 要发送的一个字节
  * 返 回 值：无
  */
void Serial2_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte);		//将字节数据写入数据寄存器，写入后USART自动生成时序波形
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);	//等待发送完成
	/*下次写入数据寄存器会自动清除发送完成标志位，故此循环后，无需清除标志位*/
}




/**
  * 函    数：次方函数（内部使用）
  * 返 回 值：返回值等于X的Y次方
  */
uint32_t Serial2_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//设置结果初值为1
	while (Y --)			//执行Y次
	{
		Result *= X;		//将X累乘到结果
	}
	return Result;
}

/**
  * 函    数：串口发送数字
  * 参    数：Number 要发送的数字，范围：0~4294967295
  * 参    数：Length 要发送数字的长度，范围：0~10
  * 返 回 值：无
  */
void Serial2_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)		//根据数字长度遍历数字的每一位
	{
		Serial2_SendByte(Number / Serial2_Pow(10, Length - i - 1) % 10 + '0');	//依次调用Serial_SendByte发送每位数字
	}
}



/**
  * 函    数：取一个已收到的字节（标志位和数据一起读，原子）
  * 参    数：out 用来带回数据
  * 返 回 值：1 = 取到了，0 = 没有新字节
  */
uint8_t Serial2_TakeRxData(uint8_t *out)
{
    uint8_t got = 0;
    // 原来那种 GetRxFlag() + GetRxData() 是两次调用，中间可以被打断：标志位还是
    // 1 但数据已经被后一个字节盖掉了 —— 前一个字节静默消失；反过来标志位还没
    // 置上就先读了数据，同一个字节会被执行两次（'P' 画两次车位）。
    // 两边都圈在临界区里，读到的数据和清标志一定配套。
    __disable_irq();
    if (Serial2_RxFlag) {
        *out = Serial2_RxData;
        Serial2_RxFlag = 0;
        got = 1;
    }
    __enable_irq();
    return got;
}
/**
  * 函    数：USART2中断函数
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
  *           函数名为预留的指定名称，可以从启动文件复制
  *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
  */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)		//判断是否是USART2的接收事件触发的中断
	{
		Serial2_RxData = USART_ReceiveData(USART2);				//读取数据寄存器，存放在接收的数据变量
		Serial2_RxFlag = 1;										//置接收标志位变量为1
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);			//清除USART2的RXNE标志位
																//读取数据寄存器会自动清除此标志位
																//如果已经读取了数据寄存器，也可以不执行此代码
	}
}

