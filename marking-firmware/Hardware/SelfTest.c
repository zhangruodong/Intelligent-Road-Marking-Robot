#include "stm32f10x.h"                  // Device header
#include  "SelfTest.h"
#include  "DPWM.h"
#include  "Servo.h"
#include "Delay.h"


#include <stdlib.h>
#include <math.h> 
#include "SelfTest.h"

void  Self_Check_Routine(void) //自检
{
	//由关到开
        Servo_StartSmoothMotion(0, 40.0f, 80.0f, 2000);//Pb6角度越大张开越大左舵机
        Servo_StartSmoothMotion(1, 90.0f, 50.0f, 2000);//PB7越小越大
        
        while(Servo_IsMoving(0) || Servo_IsMoving(1)) {
            Servo_UpdateAllMotions();
            Delay_ms(10);
        }
        Delay_s(1);
	
    //由开到关
	// 启动双舵机运动（2000ms持续时间）
        Servo_StartSmoothMotion(0, 80.0f, 40.0f, 2000); // 舵机1
        Servo_StartSmoothMotion(1, 50.0f, 90.0f, 2000);  // 舵机2
		
        // 等待运动完成
        while(Servo_IsMoving(0) || Servo_IsMoving(1)) {
            Servo_UpdateAllMotions();
            Delay_ms(10); // 10ms更新周期
        }
        Delay_s(1);
        
		//由关到开
        Servo_StartSmoothMotion(0, 40.0f, 80.0f, 2000);
        Servo_StartSmoothMotion(1, 90.0f, 50.0f, 2000);
        
        while(Servo_IsMoving(0) || Servo_IsMoving(1)) {
            Servo_UpdateAllMotions();
            Delay_ms(10);
        }
        Delay_s(1);
    }
