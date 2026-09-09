#include "stm32f10x.h"                  // Device header

static uint8_t tim4_initialized = 0;    // 标志TIM4是否已初始化


void TIM4_Init(void)
{
    if (tim4_initialized) return;       
    tim4_initialized = 1;

    // 开启TIM4时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); 
    
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;   // PWM周期20ms (72MHz/72=1MHz)
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;   // 分频后频率1MHz
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
}

// 初始化PWM通道（PB6 TIM4通道1）
void DPWM1_Init(void)
{
    TIM4_Init(); // 确保时基已配置

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置PB6为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 配置通道1为PWM模式
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; // 初始占空比0%
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    
    // 启用预装载功能
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    
    // 最后启动定时器
    TIM_Cmd(TIM4, ENABLE);
}

// 设置通道1占空比
void DPWM_SetCompare1(uint16_t Compare)
{
    TIM_SetCompare1(TIM4, Compare); 
}

// 初始化PWM通道（PB7  TIM4通道2）
void DPWM2_Init(void)
{
    TIM4_Init(); 

    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置PB7为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 配置通道2为PWM模式
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);
    
    // 启用预装载功能
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    
    // 启动定时器
    TIM_Cmd(TIM4, ENABLE);
}

// 设置通道2占空比
void DPWM2_SetCompare2(uint16_t Compare)
{
    TIM_SetCompare2(TIM4, Compare);
}
