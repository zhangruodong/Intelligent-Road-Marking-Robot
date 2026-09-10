#include "StepMotor_New.h"

// ---------- 脉冲计数变量 ----------
static volatile uint32_t Motor_PUL_SET[4] = {0, 0, 0, 0};
static volatile uint32_t Motor_PUL_CNT[4] = {0, 0, 0, 0};

// ---------- 速度档位（仅蓝牙 '+' / '-' 可调） ----------
// ARR 越小越快：TIM3 计数时钟 1MHz，ARR=300 → 3.3kHz，ARR=1000 → 1kHz
#define SPEED_FAST_ARR     150   // 最快档（ARR 下限，约 6.7kHz）
#define SPEED_SLOW_ARR    1500   // 最慢档（ARR 上限，约 667Hz）
#define SPEED_STEP_ARR     100   // 每按一次增减的步长
#define SPEED_DEFAULT_ARR  300   // 上电默认档（与旧版巡航速度一致）

static volatile uint16_t speed_arr = SPEED_DEFAULT_ARR;   // 当前速度档位

// ---------- 软起软停（固定斜坡，不随行程变） ----------
// 起步从 RAMP_SOFT_ARR 一档档加到 speed_arr，收尾再一档档退回。
// 巡航速度永远等于档位速度；行程不够就自动走成三角形（加到一半直接转减速）。
#define RAMP_SOFT_ARR         1800   // 起停两端的 ARR（越大越柔，1.8kHz 起步）
#define RAMP_SOFT_STEP_ARR     100   // 每次调整的 ARR 步长
#define RAMP_SOFT_STEP_PULSE   100   // 每多少个脉冲调整一次

#define RAMP_ACCEL    0
#define RAMP_CRUISE   1
#define RAMP_DECEL    2

static volatile uint16_t current_arr = 1000;        // 当前 ARR，由 SetFrequency 维护
static volatile uint8_t  ramp_on     = 0;           // 本次运动是否在做斜坡
static volatile uint8_t  ramp_state  = RAMP_ACCEL;  // 加速 / 匀速 / 减速
static volatile uint16_t ramp_period = 0;           // 斜坡节拍计数
static volatile uint32_t ramp_decel  = 0;           // 减速段长度（脉冲数）
static volatile uint8_t  ramp_enable = 1;           // 0 = 本次运动不起坡（画线用）

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

// ---------- 开关斜坡：包在运动调用外面用，用完立刻恢复，不留跨指令的状态 ----------
void StepMotor_SetRampEnabled(uint8_t en)
{
    ramp_enable = en;
}

// ---------- 斜坡初始化：每次运动启动时由第一路电机调用一次 ----------
static void StepMotor_RampStart(uint32_t pulse)
{
    uint32_t steps;

    if (!ramp_enable || speed_arr >= RAMP_SOFT_ARR) {   // 斜坡被关掉，或档位本来就慢
        ramp_on = 0;
        StepMotor_SetFrequency(speed_arr);
        return;
    }

    steps      = (uint32_t)(RAMP_SOFT_ARR - speed_arr) / RAMP_SOFT_STEP_ARR;
    ramp_decel = steps * RAMP_SOFT_STEP_PULSE;
    if (ramp_decel * 2 > pulse) {            // 行程短，走成三角形
        ramp_decel = pulse / 2;
    }
    ramp_period = 0;
    ramp_state  = RAMP_ACCEL;
    ramp_on     = 1;
    StepMotor_SetFrequency(RAMP_SOFT_ARR);   // 从最慢起步
}

// ---------- 蓝牙调速：每按一次走一档，正在运动时立即生效 ----------
// ARR 越小越快，所以"加速"是减小 ARR
void StepMotor_SpeedUp(void)
{
    if (speed_arr > SPEED_FAST_ARR + SPEED_STEP_ARR) {
        speed_arr -= SPEED_STEP_ARR;
    } else {
        speed_arr = SPEED_FAST_ARR;
    }
    // 只在没起坡的时候直接写 ARR。运动中写下去等于把 current_arr 一步设成
    // 目标值，斜坡里那句「current_arr > speed_arr」当场变成假，下一拍就切
    // CRUISE —— 从 1800(=555Hz) 一个周期内跳到 150(=5kHz)，九倍速的突变。
    // 开环步进没有编码器，这一下丢的步再也补不回来。
    // 只改 speed_arr 当目标，中断里那套斜坡会自己一档档走过去
    // （每 RAMP_SOFT_STEP_PULSE 个脉冲走 RAMP_SOFT_STEP_ARR）。
    if (!ramp_on) {
        StepMotor_SetFrequency(speed_arr);
    }
}

void StepMotor_SpeedDown(void)
{
    if (speed_arr < SPEED_SLOW_ARR - SPEED_STEP_ARR) {
        speed_arr += SPEED_STEP_ARR;
    } else {
        speed_arr = SPEED_SLOW_ARR;
    }
    // 只在没起坡的时候直接写 ARR。运动中写下去等于把 current_arr 一步设成
    // 目标值，斜坡里那句「current_arr > speed_arr」当场变成假，下一拍就切
    // CRUISE —— 从 1800(=555Hz) 一个周期内跳到 150(=5kHz)，九倍速的突变。
    // 开环步进没有编码器，这一下丢的步再也补不回来。
    // 只改 speed_arr 当目标，中断里那套斜坡会自己一档档走过去
    // （每 RAMP_SOFT_STEP_PULSE 个脉冲走 RAMP_SOFT_STEP_ARR）。
    // 降速这边只跳一档，本身不危险，但同理：直接写会把正在走的斜坡
    // 打断成 CRUISE，加速段白跑。走斜坡更平滑。
    if (!ramp_on) {
        StepMotor_SetFrequency(speed_arr);
    }
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

    // 5.1 使能预装载：ARR/CCR 在下一次更新事件才生效，避免改频瞬间产生残缺脉冲而丢步
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);

    // 5.2 初始化时先关闭四路 PWM 输出，避免上电电机自走。
    //     驱动器需要的首脉冲，由 Hardware_Init 里的 StepMotor_SetPulse(MOTORx, 1) 显式给出
    TIM3->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E);

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
void StepMotor_SetPulse(uint8_t motor, uint32_t pulse)
{
    if (pulse == 0) {
        StepMotor_Stop(motor);
        return;
    }

    uint16_t enableBit = GetCCER_EnableBit(motor);

    // SET / CNT / 通道使能必须一次写完，不能只锁最后那句 CCER 写。
    // TIM3 中断是「CNT++ >= SET 就关通道并把 SET 清零」，而 CNT 里上一段的
    // 残余值不会因为换段而变小：中断只要挤在 SET 和 CNT 这两句之间进来，
    // 就会拿旧的大 CNT 跟新 SET 一比、当场判"这段跑完了"，把 SET 清 0。
    // 然后这里在 SET==0 的情况下把通道使能出去 —— 中断的守卫是 `SET > 0`，
    // 从此再没有谁会关它，那一路就一直在转，而 IsAnyRunning() 报的是"空闲"。
    // 起坡也一起圈进来：它的 ramp_on 同样被中断读写，放外面会被当成
    // remaining==0 清掉。这一段都是寄存器写，占中断的时间可以忽略。
    __disable_irq();

    uint8_t was_idle = (Motor_PUL_SET[0] | Motor_PUL_SET[1] |
                        Motor_PUL_SET[2] | Motor_PUL_SET[3]) == 0;

    Motor_PUL_SET[motor-1] = pulse;
    Motor_PUL_CNT[motor-1] = 0;

    // 第一路起坡；后面几路保持频率不动，免得把斜坡打断
    if (was_idle) {
        StepMotor_RampStart(pulse);
    }

    TIM3->CCER |= enableBit;
    __enable_irq();
}

// ---------- 停止单个电机 ----------
void StepMotor_Stop(uint8_t motor)
{
    uint16_t enableBit = GetCCER_EnableBit(motor);
    __disable_irq();   // 同上，保证停机动作不会被中断抢掉
    TIM3->CCER &= ~enableBit;
    __enable_irq();
    Motor_PUL_SET[motor-1] = 0;
    Motor_PUL_CNT[motor-1] = 0;
    ramp_on = 0;                     // 被主动停掉，斜坡作废

}

// ---------- 停止所有电机 ----------
void StepMotor_StopAll(void)
{
    for (uint8_t i = 1; i <= 4; i++) {
        StepMotor_Stop(i);
    }
}

// ---------- 遥控点动的柔和停车：把"无限"改成"再走一段减速距离" ----------
// 直接 StopAll 会猛地一顿，这里复用已有的斜坡状态机把速度压下来。
// 幂等：已经是有限值的不动，所以看门狗超时后每毫秒调一次也没关系。
void StepMotor_StopJog(void)
{
    if (!ramp_on) {          // 没在起坡（本来就是慢档），别拖，直接停
        StepMotor_StopAll();
        return;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if (Motor_PUL_SET[i] == JOG_PULSE_INFINITE) {
            Motor_PUL_SET[i] = Motor_PUL_CNT[i] + ramp_decel;
        }
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

        // 2. 软起软停：一档档推进 ARR，消掉起停瞬间的速度跳变
        if (ramp_on) {
            uint32_t remaining = 0;
            for (uint8_t i = 0; i < 4; i++) {
                if (Motor_PUL_SET[i] > 0) {
                    remaining = Motor_PUL_SET[i] - Motor_PUL_CNT[i];
                    break;
                }
            }
            if (remaining == 0) {
                ramp_on = 0;
            } else if (++ramp_period >= RAMP_SOFT_STEP_PULSE) {
                ramp_period = 0;
                if (ramp_state == RAMP_ACCEL) {
                    if (remaining <= ramp_decel) {
                        ramp_state = RAMP_DECEL;          // 行程不够，直接收尾
                    } else if (current_arr > speed_arr) {
                        uint16_t a = current_arr - RAMP_SOFT_STEP_ARR;
                        if (a < speed_arr) a = speed_arr;
                        StepMotor_SetFrequency(a);
                    } else {
                        ramp_state = RAMP_CRUISE;
                    }
                } else if (ramp_state == RAMP_CRUISE) {
                    if (remaining <= ramp_decel) ramp_state = RAMP_DECEL;
                } else {   // RAMP_DECEL
                    if (current_arr < RAMP_SOFT_ARR) {
                        uint16_t a = current_arr + RAMP_SOFT_STEP_ARR;
                        if (a > RAMP_SOFT_ARR) a = RAMP_SOFT_ARR;
                        StepMotor_SetFrequency(a);
                    }
                }
            }
        }

        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

// ---------- 运动函数（设置运动类型） ----------

// 前进（直线）
void StepMotor_APPROACH(void)
{
    StepMotor_SetDirection(MOTOR1, M1_FWD);
    StepMotor_SetDirection(MOTOR2, M2_FWD);
    StepMotor_SetDirection(MOTOR3, M3_FWD);
    StepMotor_SetDirection(MOTOR4, M4_FWD);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}

// 后退（直线）
void StepMotor_RETREAT(void)
{
    StepMotor_SetDirection(MOTOR1, M1_REV);
    StepMotor_SetDirection(MOTOR2, M2_REV);
    StepMotor_SetDirection(MOTOR3, M3_REV);
    StepMotor_SetDirection(MOTOR4, M4_REV);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}

// 右转（原地旋转）
void StepMotor_You(void)
{
    StepMotor_SetDirection(MOTOR1, DIR_CW);
    StepMotor_SetDirection(MOTOR2, DIR_CW);
    StepMotor_SetDirection(MOTOR3, DIR_CW);
    StepMotor_SetDirection(MOTOR4, DIR_CW);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}

// 左转（原地旋转）
void StepMotor_Zuo(void)
{
    StepMotor_SetDirection(MOTOR1, DIR_CCW);
    StepMotor_SetDirection(MOTOR2, DIR_CCW);
    StepMotor_SetDirection(MOTOR3, DIR_CCW);
    StepMotor_SetDirection(MOTOR4, DIR_CCW);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}

// 左移（直线运动，平移）
void StepMotor_MOVE_LEFT(void)
{
    StepMotor_SetDirection(MOTOR1, DIR_CCW);
    StepMotor_SetDirection(MOTOR2, DIR_CCW);
    StepMotor_SetDirection(MOTOR3, DIR_CW);
    StepMotor_SetDirection(MOTOR4, DIR_CW);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}

// 右移（直线运动，平移）
void StepMotor_MOVE_RIGHT(void)
{
    StepMotor_SetDirection(MOTOR1, DIR_CW);
    StepMotor_SetDirection(MOTOR2, DIR_CW);
    StepMotor_SetDirection(MOTOR3, DIR_CCW);
    StepMotor_SetDirection(MOTOR4, DIR_CCW);
    StepMotor_SetPulse(MOTOR1, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR2, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR3, JOG_PULSE_INFINITE);
    StepMotor_SetPulse(MOTOR4, JOG_PULSE_INFINITE);
}
// 直线前进指定脉冲数（四个电机同步）
void StepMotor_GoForward(uint16_t pulse)
{
    StepMotor_SetDirection(MOTOR1, M1_FWD);
    StepMotor_SetDirection(MOTOR2, M2_FWD);
    StepMotor_SetDirection(MOTOR3, M3_FWD);
    StepMotor_SetDirection(MOTOR4, M4_FWD);
    StepMotor_SetPulse(MOTOR1, pulse);
    StepMotor_SetPulse(MOTOR2, pulse);
    StepMotor_SetPulse(MOTOR3, pulse);
    StepMotor_SetPulse(MOTOR4, pulse);
}

// 画线专用：匀速走直线，不做软起软停。
// 起坡段车速只有巡航的 1/1.75，而泵的出漆量恒定 —— 走得慢就喷得厚，
// 会让每条线的头尾 0.3m 明显比中段厚，所以画线这几段必须匀速。
void StepMotor_GoForwardFlat(uint16_t pulse)
{
    StepMotor_SetRampEnabled(0);
    StepMotor_GoForward(pulse);
    StepMotor_SetRampEnabled(1);
}

// 原地右转指定脉冲数（四个电机同向）
void StepMotor_TurnRight(uint16_t pulse)
{
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
