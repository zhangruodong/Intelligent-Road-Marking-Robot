#ifndef __SYSTEM_H
#define __SYSTEM_H
#include "stm32f10x.h"
#include <stdlib.h>
#include <math.h> 
#include "string.h"
#include <stdio.h>
#include <stdbool.h>
#include "OLED.h"
#include "USART.h"
#include "string.h"
#include  "PumpBuzzer.h"
#include  "Servo.h"
#include  "StepMotor_New.h"
/* 系统状态枚举 */


typedef enum {
    STATE_WAIT = 'W',   //
		STATE_KAI='K',//水泵开
		STATE_GUAN='G',//水泵关
		STATE_SELFCHECK='D',//整机自检（舵机动一下 + 前进一点）
		STATE_GSERVO='J',//夹臂关
    STATE_RETREAT_BEEP = 'F',// 提示
    STATE_APPROACH = 'U',//前进
    STATE_RETREAT = 'R',    // 后退
		STATE_YOU='Y',//右转
		STATE_ZUO='Z',//左转
	  STATE_MOVE_LEFT = 'A',    // 左移
    STATE_MOVE_RIGHT = 'X',    // 右移
		STATE_TING='T',//急停,
	  STATE_PAINT = 'P',   //
	  STATE_RESPRAY = 'C'  // 二次喷涂补线（相机检测到破损时上位机发）
} SystemState;
typedef struct {
    SystemState state;           // 当前系统状态
		SystemState last_state;      // 上一次的状态（用于检测进入）//
    uint32_t state_timestamp;    // 状态进入时间戳（毫秒）
} SystemCtrl;


void System_StateMachine(void);
uint32_t GetTick(void);
void Hardware_Init(void);
void TIM1_Init(void);
void GPIO15_Init(void);
void TIM1_UP_IRQHandler(void);
void ProcessCommand(void) ;



#endif
