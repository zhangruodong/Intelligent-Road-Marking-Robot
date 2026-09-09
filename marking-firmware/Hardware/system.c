#include "stm32f10x.h"
#include "system.h"
uint8_t cmd=0 ;
uint8_t cmd_from = 0;   // 0:无, 1:Serial1, 2:Serial2
// 画长方形脉冲数（标准车位 5.3m × 2.5m）
#define PAINT_LONG_PULSE   26500
#define PAINT_SHORT_PULSE  12500
#define PAINT_TURN_PULSE   3000

// 画线子状态枚举
typedef enum {
    PAINT_IDLE = 0,
    PAINT_EDGE1,
    PAINT_TURN1,
    PAINT_EDGE2,
    PAINT_TURN2,
    PAINT_EDGE3,
    PAINT_TURN3,
    PAINT_EDGE4,
    PAINT_DONE
} PaintStep;
static PaintStep paint_step = PAINT_IDLE;
// 全局变量用于记录定时器中断次数
volatile uint32_t timer_counter = 0;

static SystemCtrl sys_ctrl = {STATE_WAIT, STATE_WAIT, 0, 0};

// TIM1初始化函数
void TIM1_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 1. 使能TIM1时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* 2. 配置定时器参数 */
    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1;     // 预分频值（72MHz / 7200 = 10KHz）
    TIM_TimeBaseStructure.TIM_Period = 10 - 1;       // 自动重装载值（10KHz / 10000 = 1秒中断一次）
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;    // 高级定时器特有参数
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* 3. 使能定时器更新中断 */
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

    /* 4. 配置NVIC中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 5. 启动定时器 */
    TIM_Cmd(TIM1, ENABLE);
}

// GPIO初始化
void GPIO15_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    /* 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

// TIM1更新中断服务函数
void TIM1_UP_IRQHandler(void) {
    if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM1, TIM_IT_Update); // 清除中断标志
        timer_counter++;
        GPIO_WriteBit(GPIOB, GPIO_Pin_15, 
                     (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_15))); // 翻转PB15
    }
}

void Hardware_Init(void)  //外设初始化
{
	Servo_Init();		  //舵机初始化
	Servo2_Init();
	Serial2_Init();
	Pump_Init();		  //水泵初始化
	Buzzer_Init();			  //蜂鸣器初始化
	OLED_Init();
	Serial1_Init();
	GPIO15_Init();
	TIM1_Init();
	Serial3_Init();
	StepMotor_Init();   
StepMotor_SetPulse(MOTOR1, 1);           
StepMotor_SetPulse(MOTOR2, 1);
StepMotor_SetPulse(MOTOR3, 1);
StepMotor_SetPulse(MOTOR4, 1);
}

void ProcessCommand(void) {
  if(Serial1_GetRxFlag()) {
        cmd = Serial1_GetRxData();
        cmd_from = 1;
    }
    else if(Serial2_GetRxFlag()) {
        cmd = Serial2_GetRxData();
        cmd_from = 2;
    }

    if(cmd_from != 0) {
			  //Serial1_SendByte(cmd);
        Serial2_SendByte(cmd);
        sys_ctrl.last_cmd = cmd;
//	if(Serial1_GetRxFlag()) {
//        cmd = Serial1_GetRxData();
//		Serial1_SendByte(cmd);
//sys_ctrl.last_cmd = cmd;  // 存储最新指令
        switch(cmd) {
			
		case 'W'://等待指令  串口'W'
			    StepMotor_StopAll();
					OLED_ShowString(3, 1, "Wait    ");
        sys_ctrl.state = STATE_WAIT;
				break;
		case 'K'://开水泵
        sys_ctrl.state = STATE_KAI;
				break;
		case 'G'://水泵关
				sys_ctrl.state = STATE_GUAN;
				break;
		case 'D'://夹臂开
				sys_ctrl.state = STATE_KSERVO;
				break;
		case 'J'://舵机关
				sys_ctrl.state = STATE_GSERVO;
				break;
		case 'F':	// 蜂鸣提示	'F'
        sys_ctrl.state = STATE_RETREAT_BEEP;
        break;
		/*--------------------------------------------------------------------------------------------------*/
    case 'U':		//前进'U'
        sys_ctrl.state = STATE_APPROACH;
        break;
    case 'R':			//后撤	'R'
        sys_ctrl.state = STATE_RETREAT;
        break;
		case 'Y'://右转
        sys_ctrl.state = STATE_YOU;
				break;
		case 'Z'://左转
        sys_ctrl.state = STATE_ZUO;
				break;
		case 'A': //左移
				sys_ctrl.state = STATE_MOVE_LEFT; 
				break;
    case 'X': //右移
				sys_ctrl.state = STATE_MOVE_RIGHT; 
				break;
		case 'T'://强制停止
        sys_ctrl.state = STATE_TING;
				break;
		case 'P'://画车位
    sys_ctrl.state = STATE_PAINT;
    break;
        }
    }
}
uint32_t GetTick(void) {
    return timer_counter;  
}
void System_StateMachine(void) {
    uint32_t current_time = GetTick();
    ProcessCommand();
	 // 检测状态是否改变
    if (sys_ctrl.state != sys_ctrl.last_state) {
        // 状态退出处理：停止所有电机（无论之前是什么状态）
        // 注意：如果新状态本身需要电机运动，稍后会重新启动
        StepMotor_StopAll();

        // 记录新状态进入时间
        sys_ctrl.state_timestamp = current_time;

        // 更新 last_state
        sys_ctrl.last_state = sys_ctrl.state;
    switch(sys_ctrl.state) {
    case STATE_WAIT:   
				StepMotor_StopAll();
		   OLED_ShowString(3, 1, "Wait    ");
					break;
    case STATE_APPROACH:
				OLED_ShowString(3, 1, "Approach  ");
				StepMotor_APPROACH();
					break;
		case STATE_RETREAT:    // 后退
        OLED_Clear();
        OLED_ShowString(3, 1, "Retreat Done  ");
				StepMotor_RETREAT();
//        if(current_time - sys_ctrl.state_timestamp > 30000) {
//            sys_ctrl.state = STATE_RETREAT_BEEP;
//            sys_ctrl.state_timestamp = current_time;
//        }
        break;
		
	   case STATE_YOU:        // 右转（原地顺时针）
        OLED_ShowString(3, 1, "YOU ");
				StepMotor_You();
        break;
		 

    case STATE_ZUO:        // 左转（原地逆时针）
        OLED_ShowString(3, 1, "ZUO ");
				StepMotor_Zuo();
        break;

	
		case STATE_MOVE_LEFT:   
				StepMotor_MOVE_LEFT();
        break;
		
		case STATE_MOVE_RIGHT:   
				StepMotor_MOVE_RIGHT();
        break;
		case STATE_PAINT:
{// 每次进入画线状态都从头开始
				 paint_step = PAINT_IDLE; 
    switch (paint_step) {
        case PAINT_IDLE:
            Pump_On();
            StepMotor_GoForward(PAINT_LONG_PULSE);
            paint_step = PAINT_EDGE1;
            OLED_ShowString(3, 1, "Paint:1/4");
            break;
        case PAINT_EDGE1:
            if (!StepMotor_IsAnyRunning()) {
                Pump_Off();
                StepMotor_TurnRight(PAINT_TURN_PULSE);
                paint_step = PAINT_TURN1;
                OLED_ShowString(3, 1, "Turn1     ");
            }
            break;
        case PAINT_TURN1:
            if (!StepMotor_IsAnyRunning()) {
                Pump_On();
                StepMotor_GoForward(PAINT_SHORT_PULSE);
                paint_step = PAINT_EDGE2;
                OLED_ShowString(3, 1, "Paint:2/4");
            }
            break;
        case PAINT_EDGE2:
            if (!StepMotor_IsAnyRunning()) {
                Pump_Off();
                StepMotor_TurnRight(PAINT_TURN_PULSE);
                paint_step = PAINT_TURN2;
                OLED_ShowString(3, 1, "Turn2     ");
            }
            break;
        case PAINT_TURN2:
            if (!StepMotor_IsAnyRunning()) {
                Pump_On();
                StepMotor_GoForward(PAINT_LONG_PULSE);
                paint_step = PAINT_EDGE3;
                OLED_ShowString(3, 1, "Paint:3/4");
            }
            break;
        case PAINT_EDGE3:
            if (!StepMotor_IsAnyRunning()) {
                Pump_Off();
                StepMotor_TurnRight(PAINT_TURN_PULSE);
                paint_step = PAINT_TURN3;
                OLED_ShowString(3, 1, "Turn3     ");
            }
            break;
        case PAINT_TURN3:
            if (!StepMotor_IsAnyRunning()) {
                Pump_On();
                StepMotor_GoForward(PAINT_SHORT_PULSE);
                paint_step = PAINT_EDGE4;
                OLED_ShowString(3, 1, "Paint:4/4");
            }
            break;
        case PAINT_EDGE4:
            if (!StepMotor_IsAnyRunning()) {
                Pump_Off();
                paint_step = PAINT_DONE;
                sys_ctrl.state = STATE_WAIT;
                OLED_ShowString(3, 1, "Paint Done");
            }
            break;
        default:
            break;
    }
    break;
}
		
	  case STATE_TING:       // 强制停止
        Pump_Off();
				StepMotor_StopAll();
		    OLED_ShowString(3, 1, "STOPPED "); 
        break;
		case STATE_KAI://开水泵
					Pump_On();
					OLED_ShowString(3, 1, "KAI ");
					break;
		case STATE_GUAN://关水泵
					Pump_Off();
					OLED_ShowString(3, 1, "GUAN ");
					break;
		case STATE_KSERVO://张开舵机
//			//由关到开
					Servo_StartSmoothMotion(0, 40.0f, 80.0f, 2000);
					Servo_StartSmoothMotion(1, 90.0f, 50.0f, 2000);
					while(Servo_IsMoving(0) || Servo_IsMoving(1)) {
            Servo_UpdateAllMotions();
            Delay_ms(10);
					}
					Delay_s(1);
					OLED_ShowString(3, 1, "KSERVO ");
					sys_ctrl.state = STATE_WAIT;
					break;
//					Servo_StartSmoothMotion(0, 40.0f, 80.0f, 2000);
//					Servo_StartSmoothMotion(1, 90.0f, 50.0f, 2000);
//					OLED_ShowString(3, 1, "KSERVO ");
//					break;
			case STATE_GSERVO://关闭舵机
				
						Servo_StartSmoothMotion(0, 80.0f, 40.0f, 2000); // 舵机1
						Servo_StartSmoothMotion(1, 50.0f, 90.0f, 2000);  // 舵机2
		
        // 等待运动完成
						while(Servo_IsMoving(0) || Servo_IsMoving(1)) {
								Servo_UpdateAllMotions();
							Delay_ms(10); // 10ms更新周期
						}
						Delay_s(1);
						sys_ctrl.state = STATE_WAIT;
						OLED_ShowString(3, 1, "GSERVO");
						break;
			
////关闭舵机（非阻塞启动）
//						Servo_StartSmoothMotion(0, 80.0f, 40.0f, 2000);
//						Servo_StartSmoothMotion(1, 50.0f, 90.0f, 2000);
//						OLED_ShowString(3, 1, "GSERVO");
//						break;
			
			case STATE_RETREAT_BEEP://蜂鸣器，结束后到等待状态
						Buzzer_On();
						OLED_Clear();
//						Delay_ms(500);
						OLED_ShowString(3, 1, "FMQ ");
//        if(current_time - sys_ctrl.state_timestamp > 2000) {
//            Buzzer_Off();
//            sys_ctrl.state = STATE_WAIT;
//        }
        break;
				  default:
                break;
    }
		//cmd_from = 0; 
}else {
        // 状态未改变，执行周期性任务（如超时判断）
        switch (sys_ctrl.state) {
            case STATE_RETREAT_BEEP:
                if (current_time - sys_ctrl.state_timestamp > 2000) {
                    Buzzer_Off();
                    sys_ctrl.state = STATE_WAIT;
                }
                break;

            case STATE_PAINT:
            {
                switch (paint_step) {
                    case PAINT_EDGE1:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            StepMotor_TurnRight(PAINT_TURN_PULSE);
                            paint_step = PAINT_TURN1;
                            OLED_ShowString(3, 1, "Turn1     ");
                        }
                        break;
                    case PAINT_TURN1:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForward(PAINT_SHORT_PULSE);
                            paint_step = PAINT_EDGE2;
                            OLED_ShowString(3, 1, "Paint:2/4");
                        }
                        break;
                    case PAINT_EDGE2:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            StepMotor_TurnRight(PAINT_TURN_PULSE);
                            paint_step = PAINT_TURN2;
                            OLED_ShowString(3, 1, "Turn2     ");
                        }
                        break;
                    case PAINT_TURN2:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForward(PAINT_LONG_PULSE);
                            paint_step = PAINT_EDGE3;
                            OLED_ShowString(3, 1, "Paint:3/4");
                        }
                        break;
                    case PAINT_EDGE3:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            StepMotor_TurnRight(PAINT_TURN_PULSE);
                            paint_step = PAINT_TURN3;
                            OLED_ShowString(3, 1, "Turn3     ");
                        }
                        break;
                    case PAINT_TURN3:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForward(PAINT_SHORT_PULSE);
                            paint_step = PAINT_EDGE4;
                            OLED_ShowString(3, 1, "Paint:4/4");
                        }
                        break;
                    case PAINT_EDGE4:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            paint_step = PAINT_DONE;
                            sys_ctrl.state = STATE_WAIT;
                            OLED_ShowString(3, 1, "Paint Done");
                        }
                        break;
                    default:
                        break;
//                }
//                break;
//            }
//            // 舵机状态非阻塞处理（每10ms更新一次平滑运动）
////            case STATE_KSERVO:
////            case STATE_GSERVO:
////            {
////                static uint32_t last_servo_update = 0;
////                if (current_time - last_servo_update >= 10) {
////                    Servo_UpdateAllMotions();
////                    last_servo_update = current_time;
////                }
////                if (!Servo_IsMoving(0) && !Servo_IsMoving(1)) {
////                    // 运动完成，显示提示并返回等待状态
////                    OLED_ShowString(3, 1, (sys_ctrl.state == STATE_KSERVO) ? "KSERVO Done" : "GSERVO Done");
////                    sys_ctrl.state = STATE_WAIT;
               }
                break;
            }
            default:
                break;
        }
    }
}
