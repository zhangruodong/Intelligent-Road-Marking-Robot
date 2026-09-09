#include "stm32f10x.h"

#define PUMP_PIN    GPIO_Pin_13
#define BUZZER_PIN  GPIO_Pin_14

// 水泵初始化
void Pump_Init(void)
{
    // 开启 GPIOB 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = PUMP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, PUMP_PIN);
}

void Pump_On(void)
{
    GPIO_SetBits(GPIOB, PUMP_PIN);
}

void Pump_Off(void)
{
    GPIO_ResetBits(GPIOB, PUMP_PIN);
}

// 蜂鸣器初始化
void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 初始关闭蜂鸣器（高电平关闭，低电平响）
    GPIO_SetBits(GPIOB, BUZZER_PIN);
}

void Buzzer_On(void)
{
    GPIO_ResetBits(GPIOB, BUZZER_PIN);  // 低电平有效
}

void Buzzer_Off(void)
{
    GPIO_SetBits(GPIOB, BUZZER_PIN);
}
