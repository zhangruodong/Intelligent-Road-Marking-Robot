#include "StepMotor_New.h"

// ---------- 脉冲计数变量 ----------
static volatile uint32_t Motor_PUL_SET[4] = {0, 0, 0, 0};
static volatile uint32_t Motor_PUL_CNT[4] = {0, 0, 0, 0};

// ---------- 运动类型与速度控制 ----------
typedef enum {
    MOTION_LINEAR,      // 直线运动（前进/后退/左右平移）
    MOTION_ROTATION     // 原地旋转（左转/右转）
} MotionType;

static volatile MotionType current_motion = MOTION_LINEAR;  // 当前运动类型
static volatile uint16_t current_arr = 800;                // 当前ARR值（初始1kHz）
static volatile uint16_t target_arr = 300;                  // 目标ARR值（10kHz）
static volatile uint8_t  acceleration_enabled = 0;          // 是否允许加速
static volatile uint32_t accel_step_counter = 0;            // 加速步数计数器
static volatile uint8_t  first_start = 1;                   // 是否首次启动（用于频率初始化）

// ---------- 辅助函数：获取对应通道的使能位 ----------
static uint16_t GetCCER_EnableBit(uint8_t motor)
{
    switch (motor) {
        case MOTOR1: return TIM_CCER_CC1E;
        case MOTOR2: return TIM_CCER_CC2E;
        case MOTOR3: return TIM_CCER_CC3E;
        case MOTOR4: return TIM_CCER_CC4E;
        default: return 0;
    }
}

// ---------- 动态修改PWM频率（保持50%占空比） ----------
void StepMotor_SetFrequency(uint16_t arr)
{
    if (arr < 2) arr = 2;                      // 最小值保护
    TIM3->ARR = arr - 1;                       // 更新自动重载值
    // 更新四个通道的比较值，保持占空比50%
    TIM3->CCR1 = (arr - 1) / 2;
    TIM3->CCR2 = (arr - 1) / 2;
    TIM3->CCR3 = (arr - 1) / 2;
    TIM3->CCR4 = (arr - 1) / 2;
    current_arr = arr;
}

// ---------- 初始化TIM3，产生四路PWM（频率可调，占空比50%） ----------
void StepMotor_Init(void)
{
    // 1. 时钟使能
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 配置方向引脚为推挽输出
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Pin = M1_DIR_PIN | M2_DIR_PIN | M3_DIR_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = M4_DIR_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. 配置脉冲引脚为复用推挽输出
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    // PA6 (CH1), PA7 (CH2)
    GPIO_InitStruct.GPIO_Pin = M1_PUL_PIN | M2_PUL_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    // PB0 (CH3), PB1 (CH4)
    GPIO_InitStruct.GPIO_Pin = M3_PUL_PIN | M4_PUL_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 4. 配置TIM3时基单元
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_Period = 1000 - 1;       // ARR，初始1kHz
    TIM_TimeBaseStruct.TIM_Prescaler = 72 - 1;      // PSC，计数频率1MHz
    TIM_TimeBaseStruct.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);

    // 5. 配置PWM输出（四个通道，占空比50%）
    TIM_OCInitTypeDef TIM_OCStruct;
    TIM_OCStructInit(&TIM_OCStruct);
    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCStruct.TIM_Pulse = 500;   // CCR = 500，占空比50%
    TIM_OC1Init(TIM3, &TIM_OCStruct);
    TIM_OC2Init(TIM3, &TIM_OCStruct);
    TIM_OC3Init(TIM3, &TIM_OCStruct);
    TIM_OC4Init(TIM3, &TIM_OCStruct);

    // 6. 清除更新标志，配置中断
    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    // 7. NVIC配置
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStruct);

    // 8. 使能TIM3
    TIM_Cmd(TIM3, ENABLE);
}

// ---------- 设置电机方向 ----------
void StepMotor_SetDirection(uint8_t motor, uint8_t dir)
{
    GPIO_TypeDef* port;
    uint16_t pin;
    switch (motor) {
        case MOTOR1: port = M1_DIR_PORT; pin = M1_DIR_PIN; break;
        case MOTOR2: port = M2_DIR_PORT; pin = M2_DIR_PIN; break;
        case MOTOR3: port = M3_DIR_PORT; pin = M3_DIR_PIN; break;
        case MOTOR4: port = M4_DIR_PORT; pin = M4_DIR_PIN; break;
        default: return;
    }
    GPIO_WriteBit(port, pin, dir ? Bit_SET : Bit_RESET);
}

// ---------- 设置指定电机输出指定数量的脉冲 ----------
void StepMotor_SetPulse(uint8_t motor, uint16_t pulse)
{
    if (pulse == 0) {
        StepMotor_Stop(motor);
        return;
    }

    Motor_PUL_SET[motor-1] = pulse;
    Motor_PUL_CNT[motor-1] = 0;

    // 如果是第一个电机启动，根据运动类型初始化频率和加速标志
    if (first_start) {
        if (current_motion == MOTION_LINEAR) {
            // 直线运动：从低速开始，允许加速
            StepMotor_SetFrequency(1000);       // 初始1kHz
            acceleration_enabled = 1;
            accel_step_counter = 0;
        } else {
            // 旋转运动：固定最慢速度，禁止加速
            StepMotor_SetFrequency(1000);       // 固定1kHz
            acceleration_enabled = 0;
        }
        first_start = 0;
    }

    // 使能对应通道的PWM输出
    uint16_t enableBit = GetCCER_EnableBit(motor);
    TIM3->CCER |= enableBit;
}

// ---------- 停止单个电机 ----------
void StepMotor_Stop(uint8_t motor)
{
    uint16_t enableBit = GetCCER_EnableBit(motor);
    TIM3->CCER &= ~enableBit;
    Motor_PUL_SET[motor-1] = 0;
    Motor_PUL_CNT[motor-1] = 0;

    // 如果所有电机都已停止，重置启动标志，以便下次运动重新初始化频率
    if (Motor_PUL_SET[0]==0 && Motor_PUL_SET[1]==0 && Motor_PUL_SET[2]==0 && Motor_PUL_SET[3]==0) {
        first_start = 1;
        acceleration_enabled = 0;
    }
}

// ---------- 停止所有电机 ----------
void StepMotor_StopAll(void)
{
    for (uint8_t i = 1; i <= 4; i++) {
        StepMotor_Stop(i);
    }
}

// ---------- TIM3更新中断处理函数 ----------
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        // 1. 处理脉冲计数
        for (uint8_t i = 0; i < 4; i++) {
            if (Motor_PUL_SET[i] > 0) {
                Motor_PUL_CNT[i]++;
                if (Motor_PUL_CNT[i] >= Motor_PUL_SET[i]) {
                    uint16_t enableBit = GetCCER_EnableBit(i+1);
                    TIM3->CCER &= ~enableBit;
                    Motor_PUL_SET[i] = 0;
                }
            }
        }

        // 2. 加速逻辑（仅直线运动且允许加速，且至少有一个电机在运行）
        if (current_motion == MOTION_LINEAR && acceleration_enabled) {
            uint8_t running = 0;
            for (uint8_t i = 0; i < 4; i++) {
                if (Motor_PUL_SET[i] > 0) {
                    running = 1;
                    break;
                }
            }
            if (running) {
                accel_step_counter++;
                // 每200个PWM周期提高一次频率（减小ARR）
                if (accel_step_counter >= 200) {
                    accel_step_counter = 0;
                    if (current_arr > target_arr) {
                        uint16_t new_arr = current_arr - 50;   // 步长可调
                        if (new_arr < target_arr) new_arr = target_arr;
                        StepMotor_SetFrequency(new_arr);
                    } else {
                        acceleration_enabled = 0; // 达到目标频率，停止加速
                    }
                }
            } else {
                acceleration_enabled = 0; // 无电机运行，关闭加速
            }
        }

        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

// ---------- 运动函数（设置运动类型） ----------

// 前进（直线）
void StepMotor_APPROACH(void)
{
    current_motion = MOTION_LINEAR;
    StepMotor_SetDirection(MOTOR1, M1_FWD);
    StepMotor_SetDirection(MOTOR2, M2_FWD);
    StepMotor_SetDirection(MOTOR3, M3_FWD);
    StepMotor_SetDirection(MOTOR4, M4_FWD);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}

// 后退（直线）
void StepMotor_RETREAT(void)
{
    current_motion = MOTION_LINEAR;
    StepMotor_SetDirection(MOTOR1, M1_REV);
    StepMotor_SetDirection(MOTOR2, M2_REV);
    StepMotor_SetDirection(MOTOR3, M3_REV);
    StepMotor_SetDirection(MOTOR4, M4_REV);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}

// 右转（原地旋转）
void StepMotor_You(void)
{
    current_motion = MOTION_ROTATION;
    StepMotor_SetDirection(MOTOR1, DIR_CW);
    StepMotor_SetDirection(MOTOR2, DIR_CW);
    StepMotor_SetDirection(MOTOR3, DIR_CW);
    StepMotor_SetDirection(MOTOR4, DIR_CW);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}

// 左转（原地旋转）
void StepMotor_Zuo(void)
{
    current_motion = MOTION_ROTATION;
    StepMotor_SetDirection(MOTOR1, DIR_CCW);
    StepMotor_SetDirection(MOTOR2, DIR_CCW);
    StepMotor_SetDirection(MOTOR3, DIR_CCW);
    StepMotor_SetDirection(MOTOR4, DIR_CCW);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}

// 左移（直线运动，平移）
void StepMotor_MOVE_LEFT(void)
{
    current_motion = MOTION_LINEAR;
    StepMotor_SetDirection(MOTOR1, DIR_CCW);
    StepMotor_SetDirection(MOTOR2, DIR_CCW);
    StepMotor_SetDirection(MOTOR3, DIR_CW);
    StepMotor_SetDirection(MOTOR4, DIR_CW);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}

// 右移（直线运动，平移）
void StepMotor_MOVE_RIGHT(void)
{
	  current_motion = MOTION_LINEAR;
    StepMotor_SetDirection(MOTOR1, DIR_CW);
    StepMotor_SetDirection(MOTOR2, DIR_CW);
    StepMotor_SetDirection(MOTOR3, DIR_CCW);
    StepMotor_SetDirection(MOTOR4, DIR_CCW);
    StepMotor_SetPulse(MOTOR1, 65535);
    StepMotor_SetPulse(MOTOR2, 65535);
    StepMotor_SetPulse(MOTOR3, 65535);
    StepMotor_SetPulse(MOTOR4, 65535);
}
// 直线前进指定脉冲数（四个电机同步）
void StepMotor_GoForward(uint16_t pulse)
{
	 first_start = 1; 
    current_motion = MOTION_LINEAR;   // 直线运动，允许加速
    StepMotor_SetDirection(MOTOR1, M1_FWD);
    StepMotor_SetDirection(MOTOR2, M2_FWD);
    StepMotor_SetDirection(MOTOR3, M3_FWD);
    StepMotor_SetDirection(MOTOR4, M4_FWD);
    StepMotor_SetPulse(MOTOR1, pulse);
    StepMotor_SetPulse(MOTOR2, pulse);
    StepMotor_SetPulse(MOTOR3, pulse);
    StepMotor_SetPulse(MOTOR4, pulse);
}

// 原地右转指定脉冲数（四个电机同向）
void StepMotor_TurnRight(uint16_t pulse)
{
	   first_start = 1; 
    current_motion = MOTION_ROTATION; // 旋转运动，固定低速
    StepMotor_SetDirection(MOTOR1, DIR_CW);
    StepMotor_SetDirection(MOTOR2, DIR_CW);
    StepMotor_SetDirection(MOTOR3, DIR_CW);
    StepMotor_SetDirection(MOTOR4, DIR_CW);
    StepMotor_SetPulse(MOTOR1, pulse);
    StepMotor_SetPulse(MOTOR2, pulse);
    StepMotor_SetPulse(MOTOR3, pulse);
    StepMotor_SetPulse(MOTOR4, pulse);
}


// 检查是否有任何电机仍在发送脉冲
uint8_t StepMotor_IsAnyRunning(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        if (Motor_PUL_SET[i] != 0) {
            return 1;
        }
    }
    return 0;
}
