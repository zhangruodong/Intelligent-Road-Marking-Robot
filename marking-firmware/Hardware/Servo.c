#include "stm32f10x.h"                  // Device header
#include "DPWM.h"
#include "Servo.h"
#include <math.h> 

extern volatile uint32_t timer_counter;
// 舵机运动实例数组
static ServoMotion servo_instances[MAX_SERVOS] = {0};

// 最后一次下发的角度。所有角度写入都收口在 Servo_SetAngle / Servo2_SetAngle，
// 在这里记一份，Servo_StartSmoothMotion 才有"当前位置"可用 ——
// 它的 start_angle 是调用方写死的，跟舵机实际位置对不上时第一帧就会猛跳。
static float servo_last_angle[MAX_SERVOS] = {0};

// 私有函数声明
static SplineCoeff calculate_spline_coeff(float y0, float y1, uint32_t duration_ms);
static float spline_interpolate(SplineCoeff coeff, float t);

// 角度钳到 0~180 才能往下算。不钳的话越界值会一路走到
// (uint16_t)(Angle / 180.0f * 2000 + 500) 那个转换里，结果是两个方向的
// 坏的极端：角度负得少（比如 -45）算出来 CCR 落到 0，输出恒低；负得多
// （比如 -90，转换本身还是 UB）算出来 CCR 超过 ARR，PWM 输出恒高。
// 两种情况都没有 500~2500us 的有效脉宽 —— 舵机收不到位置信号就没有保持
// 力矩，机械臂靠重力耷下来（0~180 之外本来也没有可用角度）。
// 钳住之后越界的指令停在端点角度上不动，全程有力矩。
// 用 !(Angle >= 0.0f) 而不是 Angle < 0.0f：顺带把 NaN 也挡了，否则
// NaN 两个比较都是假，会原样落到那个无符号转换上。
static float Servo_ClampAngle(float Angle)
{
    if (!(Angle >= 0.0f)) return 0.0f;
    if (Angle > 180.0f)   return 180.0f;
    return Angle;
}

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
    Angle = Servo_ClampAngle(Angle);
    servo_last_angle[0] = Angle;
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
    Angle = Servo_ClampAngle(Angle);
    servo_last_angle[1] = Angle;
    DPWM2_SetCompare2((uint16_t)(Angle / 180.0f * 2000 + 500));
}

// 舵机当前（最后一次下发的）角度，给平滑运动做起点用
float Servo_GetAngle(uint8_t servo_id)
{
    if (servo_id >= MAX_SERVOS) return 0.0f;
    return servo_last_angle[servo_id];
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

// 打断正在走的平滑运动：停在当前已经插值到的角度上不动。
//
// 必须显式清 is_active，不能光停止调 Servo_UpdateAllMotions()。那个函数是
// 唯一清 is_active 的地方，又只在 GSERVO/SELFCHECK 两个分支里被调 ——
// 2 秒的行程走到一半按了别的键，状态一离开，UpdateAllMotions 再没人调，
// is_active 就永远挂着 1，Servo_IsMoving() 从此一直报"还在动"。
// 谁要是写 while (Servo_IsMoving(0)); 等他停，就是个死循环。
//
// "停住"是 TIM4 比较值保持住最后那个角度，不是掉力矩 —— 所以打断之后
// 机械臂是定在原地的，这也正是想要的行为。
void Servo_StopAllMotions(void)
{
    for (uint8_t i = 0; i < MAX_SERVOS; i++) {
        servo_instances[i].is_active = 0;
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
