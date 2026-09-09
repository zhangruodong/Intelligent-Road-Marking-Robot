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
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;     // 浮空输入
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

uint8_t Serial1_GetRxFlag(void)
{
    if (Serial1_RxFlag == 1)
    {
        Serial1_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t Serial1_GetRxData(void)
{
    return Serial1_RxData;
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
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;     // 浮空输入
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
  * 函    数：获取串口接收标志位
  * 参    数：无
  * 返 回 值：串口接收标志位，范围：0~1，接收到数据后，标志位置1，读取后标志位自动清零
  */
uint8_t Serial2_GetRxFlag(void)
{
	if (Serial2_RxFlag == 1)			//如果标志位为1
	{
		Serial2_RxFlag = 0;
		return 1;					//则返回1，并自动清零标志位
	}
	return 0;						//如果标志位为0，则返回0
}

/**
  * 函    数：获取串口接收的数据
  * 参    数：无
  * 返 回 值：接收的数据，范围：0~255
  */
uint8_t Serial2_GetRxData(void)
{
	return Serial2_RxData;			//返回接收的数据变量
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


	uint8_t Serial3_RxData;		//定义串口接收的数据变量
  uint8_t Serial3_RxFlag;

void Serial3_Init(void) 
{
    /* 开启时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);    // USART3时钟在APB1上
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);     // GPIOB时钟

    /* GPIO初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // PB10作为USART3_TX（复用推挽输出）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;                // PB10
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB11作为USART3_RX（浮空输入）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;     // 浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;                // PB11
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* USART初始化 */
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART3, &USART_InitStructure);                 // USART3

    /* 中断配置 */
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);            // USART3接收中断

    /* NVIC配置 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;         // USART3中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    /* USART使能 */
    USART_Cmd(USART3, ENABLE);                                // USART3
}


void Serial3_SendByte(uint8_t Byte)
{
    USART_SendData(USART3, Byte);       
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);  
}

uint32_t Serial3_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y --)
    {
        Result *= X;
    }
    return Result;
}

void Serial3_SendNumber(uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i ++)
    {
        Serial3_SendByte(Number / Serial3_Pow(10, Length - i - 1) % 10 + '0');
    }
}

uint8_t Serial3_GetRxFlag(void)
{
    if (Serial3_RxFlag == 1)
    {
        Serial3_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t Serial3_GetRxData(void)
{
    return Serial3_RxData;
}

void USART3_IRQHandler(void)  // USART3中断处理函数
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)  // USART3
    {
        Serial3_RxData = USART_ReceiveData(USART3);         // USART3
        Serial3_RxFlag = 1;
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);    // USART3
    }
}
