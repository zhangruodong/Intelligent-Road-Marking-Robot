#ifndef __STEP_MOTOR_NEW_H
#define __STEP_MOTOR_NEW_H

#include "stm32f10x.h"

/*左前轮：左旋轮
右前轮：右旋轮
左后轮：右旋轮
右后轮：左旋轮*/
#define MOTOR1  1
#define MOTOR2  2
#define MOTOR3  3
#define MOTOR4  4

// 连续运动（手机遥控点动）：一直走到收到停止指令，不设脉冲上限。
// 不是真"无限" —— 4294967295 个脉冲在最高档也要 15 天才数完，实际等于无限。
// 兜底靠 system.c 里的遥控看门狗超时停车（蓝牙断线 / 人松手）。
#define JOG_PULSE_INFINITE  0xFFFFFFFFu

//// 方向定义基础方向宏
#define DIR_CW      0   // 正转（顺时针）
#define DIR_CCW     1   // 反转（逆时针）

// 根据实测：前轮向前用当前值，后轮向前需要取反
#define M1_FWD   DIR_CCW   // 左前
#define M2_FWD   DIR_CW    // 右前
#define M3_FWD   DIR_CW    // 左后（修正）
#define M4_FWD   DIR_CCW   // 右后（修正）

// 后退方向 = 前进方向取反
#define M1_REV   DIR_CW
#define M2_REV   DIR_CCW
#define M3_REV   DIR_CCW
#define M4_REV   DIR_CW
// 电机1：DIR=PA8, PUL=PA6 (TIM3_CH1)
#define M1_DIR_PORT     GPIOA
#define M1_DIR_PIN      GPIO_Pin_8
#define M1_PUL_PIN      GPIO_Pin_6

// 电机2：DIR=PA12, PUL=PA7 (TIM3_CH2)
#define M2_DIR_PORT     GPIOA
#define M2_DIR_PIN      GPIO_Pin_12
#define M2_PUL_PIN      GPIO_Pin_7

// 电机3：DIR=PA11, PUL=PB0 (TIM3_CH3)
#define M3_DIR_PORT     GPIOA
#define M3_DIR_PIN      GPIO_Pin_11
#define M3_PUL_PIN      GPIO_Pin_0

// 电机4：DIR=PB12, PUL=PB1 (TIM3_CH4)
#define M4_DIR_PORT     GPIOB
#define M4_DIR_PIN      GPIO_Pin_12
#define M4_PUL_PIN      GPIO_Pin_1


// 新增函数
uint8_t StepMotor_IsAnyRunning(void);           // 检查是否有电机在输出脉冲
void StepMotor_GoForward(uint16_t pulse);       // 指定脉冲数前进
void StepMotor_TurnRight(uint16_t pulse);       // 指定脉冲数右转

void StepMotor_Init(void);                         // 初始化所有电机（TIM3）
void StepMotor_SetDirection(uint8_t motor, uint8_t dir); // 设置电机方向
void StepMotor_SetPulse(uint8_t motor, uint32_t pulse);  // 设置电机脉冲数（启动输出）
void StepMotor_Stop(uint8_t motor);//电机停止转动
void StepMotor_StopAll(void);//全部停止
void StepMotor_StopJog(void);   // 遥控点动的柔和停车（减速跑完再停）
void StepMotor_SetRampEnabled(uint8_t en);      // 0 = 本次运动不起坡
void StepMotor_GoForwardFlat(uint16_t pulse);   // 匀速前进（画线用，不做软起软停）
void StepMotor_SpeedUp(void);   // 蓝牙 '+'：提速一档
void StepMotor_SpeedDown(void); // 蓝牙 '-'：降速一档

void StepMotor_APPROACH(void);
void StepMotor_RETREAT(void);
void StepMotor_You(void);
void StepMotor_Zuo(void);
void StepMotor_MOVE_LEFT(void);
void StepMotor_MOVE_RIGHT(void);
#endif
