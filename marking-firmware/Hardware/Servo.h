#ifndef __SERVO_H
#define __SERVO_H

#include <stdint.h>

// 最大支持的舵机数量
#define MAX_SERVOS 2

// 三次样条参数
typedef struct {
    float a, b, c, d;     // 三次多项式系数
} SplineCoeff;

// 舵机运动状态结构体
typedef struct {
    float start_angle;     // 起始角度(度)
    float end_angle;       // 目标角度(度)
    uint32_t duration_ms;  // 运动持续时间(ms)
    uint32_t start_time;   // 运动开始时间(ms)
    uint8_t is_active;     // 运动激活标志
    SplineCoeff coeff;     // 预计算的样条系数
} ServoMotion;

// 初始化函数
void ServoMotion_Init(void);
void Servo_Init(void);
void Servo2_Init(void);

// 基础角度设置
void Servo_SetAngle(float Angle);
void Servo2_SetAngle(float Angle);

// 平滑运动控制
void Servo_StartSmoothMotion(uint8_t servo_id, float start_angle, 
                            float end_angle, uint32_t duration_ms);
void Servo_MoveTo(uint8_t servo_id, float target_angle, uint32_t duration_ms);
void Servo_UpdateAllMotions(void);
uint8_t Servo_IsMoving(uint8_t servo_id);

#endif
