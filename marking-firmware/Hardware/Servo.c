#include "stm32f10x.h"                  // Device header
#include "DPWM.h"
#include "Servo.h"
#include <math.h> 

extern volatile uint32_t timer_counter;
// 舵机运动实例数组
static ServoMotion servo_instances[MAX_SERVOS] = {0};

// 私有函数声明
static SplineCoeff calculate_spline_coeff(float y0, float y1, uint32_t duration_ms);
static float spline_interpolate(SplineCoeff coeff, float t);

// 初始化所有舵机
void Servo_Init(void)
{
    ServoMotion_Init();
    DPWM1_Init();   // 包含TIM4_Init
}

// 舵机1角度设置（0~180°）
void Servo_SetAngle(float Angle)
{
    // 角度转PWM比较值：0°→500，180°→2500
    DPWM_SetCompare1((uint16_t)(Angle / 180.0f * 2000 + 500));
}

// 初始化舵机2
void Servo2_Init(void)
{
    ServoMotion_Init();
    DPWM2_Init();   // 包含TIM4_Init
}

// 舵机2角度设置
void Servo2_SetAngle(float Angle)
{
    DPWM2_SetCompare2((uint16_t)(Angle / 180.0f * 2000 + 500));
}

// 初始化运动状态数组
void ServoMotion_Init(void)
{
    for (int i = 0; i < MAX_SERVOS; i++) {
        servo_instances[i].is_active = 0;
    }
}

// 启动平滑运动（指定起始角度）
void Servo_StartSmoothMotion(uint8_t servo_id, float start_angle,
                             float end_angle, uint32_t duration_ms)
{
    if (servo_id >= MAX_SERVOS) return;

    // 如果持续时间为0，直接跳转到目标角度
    if (duration_ms == 0) {
        if (servo_id == 0) Servo_SetAngle(end_angle);
        else Servo2_SetAngle(end_angle);
        servo_instances[servo_id].is_active = 0;
        return;
    }

    servo_instances[servo_id].start_angle = start_angle;
    servo_instances[servo_id].end_angle = end_angle;
    servo_instances[servo_id].duration_ms = duration_ms;
    servo_instances[servo_id].start_time = timer_counter;  // 假设timer_counter由外部提供
    servo_instances[servo_id].is_active = 1;

    // 预计算样条系数
    servo_instances[servo_id].coeff = calculate_spline_coeff(start_angle, end_angle, duration_ms);
}

// 便捷函数：从当前角度平滑移动到目标角度
void Servo_MoveTo(uint8_t servo_id, float target_angle, uint32_t duration_ms)
{
    if (servo_id >= MAX_SERVOS) return;

    float current_angle;
    if (servo_id == 0) {
        // 注意：这里无法直接读取当前角度，只能从运动状态获取
        // 如果舵机正在运动，则使用其目标角度作为起始？或者获取当前计算出的角度？
        // 为了简化，若正在运动则停止当前运动，从当前插值位置开始？但当前插值位置未知。
        // 此处采用简单策略：若正在运动，则使用其起始角度（可能不准），建议上层确保调用时无运动
        current_angle = servo_instances[servo_id].is_active ? 
                        servo_instances[servo_id].start_angle : 
                        // 否则需通过某种方式读取当前PWM换算角度，这里假设可以通过外部变量获取
                        0.0f;   // 实际使用时应从编码器或保存的最后角度获取
        // 为演示，我们直接使用上次设置的角度（若未运动则使用0）
    } else {
        current_angle = servo_instances[servo_id].is_active ? 
                        servo_instances[servo_id].start_angle : 0.0f;
    }

    Servo_StartSmoothMotion(servo_id, current_angle, target_angle, duration_ms);
}

// 更新所有舵机运动（需定时调用，如每1ms）
void Servo_UpdateAllMotions(void)
{
    for (uint8_t i = 0; i < MAX_SERVOS; i++) {
        if (!servo_instances[i].is_active) continue;

        uint32_t elapsed = timer_counter - servo_instances[i].start_time;

        // 运动时间已到
        if (elapsed >= servo_instances[i].duration_ms) {
            // 设置最终角度
            if (i == 0) Servo_SetAngle(servo_instances[i].end_angle);
            else Servo2_SetAngle(servo_instances[i].end_angle);

            servo_instances[i].is_active = 0;
            continue;
        }

        // 使用预存储的系数计算插值角度
        float current_angle = spline_interpolate(servo_instances[i].coeff, (float)elapsed);

        // 设置当前角度
        if (i == 0) Servo_SetAngle(current_angle);
        else Servo2_SetAngle(current_angle);
    }
}

// 查询舵机是否正在运动
uint8_t Servo_IsMoving(uint8_t servo_id)
{
    if (servo_id >= MAX_SERVOS) return 0;
    return servo_instances[servo_id].is_active;
}

// 计算三次样条系数（自然边界，起止速度为零）
static SplineCoeff calculate_spline_coeff(float y0, float y1, uint32_t duration_ms)
{
    SplineCoeff coeff;
    float T = (float)duration_ms;  // 持续时间

    // 防止除零（调用处已保证duration_ms > 0）
    float invT = 1.0f / T;
    float invT2 = invT * invT;
    float invT3 = invT2 * invT;

    coeff.a = y0;
    coeff.b = 0.0f;                 // 起始速度0
    coeff.c = 3.0f * (y1 - y0) * invT2;
    coeff.d = -2.0f * (y1 - y0) * invT3;

    return coeff;
}

// 三次样条插值计算
static float spline_interpolate(SplineCoeff coeff, float t)
{
    // 使用乘法替代powf提高效率
    return coeff.a + coeff.b * t + coeff.c * t * t + coeff.d * t * t * t;
}
