#include "stm32f10x.h"
#include "system.h"
// 画长方形脉冲数（标准车位 5.3m × 2.5m）
#define PAINT_LONG_PULSE   26500
#define PAINT_SHORT_PULSE  12500
#define PAINT_TURN_PULSE   3000

// 整机自检前进脉冲数（约 0.4m，视轮径标定调整）
#define SELFCHECK_MOVE_PULSE  2000

// 二次喷涂一段的长度（'C'）。相机看到标线破损时上位机发一条 C，下位机
// 泵开直行这么多脉冲、跑完自动停。一段是有界的 —— 串口断了最多喷完这段。
// 按 0.2mm/脉冲算（PAINT_LONG_PULSE 26500 ≈ 车位长边 5.3m）：5000 脉冲
// ≈ 1.0m，默认速度（speed_arr=300，TIM3 1MHz → 3.3kHz）下约 1.5s。
// 现场按实际喷幅改这一个数；改完把上位机的 SPRAY_INTERVAL_S 一起改，
// 那个要略大于这里的跑完时间，短了也不会失控（上面那条判断会挡住重进），
// 只是喷得断断续续。注意 pulse 是 uint16_t，别超 65535。
#define RESPRAY_PULSE  5000

// 遥控看门狗超时（ms）：U/R/Y/Z/A/X 是手机蓝牙的点动指令，必须重复发送续命。
// 超时不刷新 = 蓝牙断了、或人已经松手 -> 自动停车。上位机走有线，不受影响。
// 手机 APP 按住按钮会重复发送的话，调到 300 最跟手；
// 按一次才发一条的 APP，得留 2000 才来得及补点。
#define JOG_TIMEOUT_MS  2000

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
static uint8_t selfcheck_step = 0;
// 全局变量用于记录定时器中断次数
volatile uint32_t timer_counter = 0;
static uint16_t led_tick = 0;   // 心跳灯计数（1ms 一次，500 次翻转 = 500ms）

// last_state 初值用非法状态，保证上电后首轮会执行 STATE_WAIT 的进入动作
static SystemCtrl sys_ctrl = {STATE_WAIT, (SystemState)0, 0};

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
        if (++led_tick >= 500) {   // 500ms 翻转一次
            led_tick = 0;
            GPIO_WriteBit(GPIOB, GPIO_Pin_15, 
                         (BitAction)(1 - GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_15)));
        }
    }
}

void Hardware_Init(void)  //外设初始化
{
	Servo_Init();		  //舵机初始化
	Servo2_Init();
	// 上电先摆到确定的合拢位：DPWM 初始 TIM_Pulse=0（占空比 0%），不写这两句
	// 舵机在收到第一条 J/D 之前完全没信号，舵臂会被外力带着乱动
	Servo_SetAngle(40.0f);
	Servo2_SetAngle(90.0f);
	Serial2_Init();
	Pump_Init();		  //水泵初始化
	Buzzer_Init();			  //蜂鸣器初始化
	OLED_Init();
	Serial1_Init();
	GPIO15_Init();
	TIM1_Init();
	StepMotor_Init();   
StepMotor_SetPulse(MOTOR1, 1);           
StepMotor_SetPulse(MOTOR2, 1);
StepMotor_SetPulse(MOTOR3, 1);
StepMotor_SetPulse(MOTOR4, 1);
}

static void HandleCmd(uint8_t cmd, uint8_t from) {
    if(from == 1) {
        Serial1_SendByte(cmd);
    } else {
        Serial2_SendByte(cmd);
    }
    // 手机点动指令（U/R/Y/Z/A/X）刷新遥控看门狗的时间戳，见 System_StateMachine。
    if (cmd=='U'||cmd=='R'||cmd=='Y'||cmd=='Z'||cmd=='A'||cmd=='X') {
        sys_ctrl.state_timestamp = timer_counter;
    }

    switch(cmd) {
			
		case 'W'://等待指令  串口'W'
			    StepMotor_StopAll();
					OLED_ShowString(3, 1, "Wait            ");
        sys_ctrl.state = STATE_WAIT;
				break;
		case 'K'://开水泵
        sys_ctrl.state = STATE_KAI;
				break;
		case 'G'://水泵关
				sys_ctrl.state = STATE_GUAN;
				break;
		case 'D'://整机自检夹臂开
				sys_ctrl.state = STATE_SELFCHECK;
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
		case '+':   // 蓝牙专用：提速一档（上位机发来忽略）
			if(from == 1) {
				StepMotor_SpeedUp();
				OLED_ShowString(3, 1, "Speed Up        ");
			}
			break;
		case '-':   // 蓝牙专用：降速一档（上位机发来忽略）
			if(from == 1) {
				StepMotor_SpeedDown();
				OLED_ShowString(3, 1, "Speed Down      ");
			}
			break;
		case 'C':// 二次喷涂
		    // 只在 WAIT 接，一条判断管三件事：
		    //  1) 已经在喷就别重进 —— 重进会把这一段从头开始，而上位机是周期重发的
		    //  2) 别打断正在跑的活（画车位/自检/夹臂都会把状态从 WAIT 带走）
		    //  3) 急停后状态停在 TING，C 进不来 —— 自己会喷的东西必须停得住
		    if (sys_ctrl.state == STATE_WAIT) {
		        sys_ctrl.state = STATE_RESPRAY;
		    }
		    break;
		case 'P'://画车位
    sys_ctrl.state = STATE_PAINT;
    break;
        }
}

void ProcessCommand(void) {
    // 蓝牙优先：两边同时有字节时先处理蓝牙；上位机那条字节还在，下一轮循环照常处理。
    // 必须用 TakeRxData：标志位和数据要在一个临界区里取，分两次读会丢字节或重执行。
    uint8_t cmd;
    if (Serial1_TakeRxData(&cmd)) HandleCmd(cmd, 1);        // 蓝牙
    else if (Serial2_TakeRxData(&cmd)) HandleCmd(cmd, 2);   // 上位机
}

uint32_t GetTick(void) {
    return timer_counter;  
}
void System_StateMachine(void) {
    ProcessCommand();
    uint32_t current_time = GetTick();
	 // 检测状态是否改变
    if (sys_ctrl.state != sys_ctrl.last_state) {
        // 状态退出处理：停止所有电机（无论之前是什么状态）
        // 注意：如果新状态本身需要电机运动，稍后会重新启动
        
        Pump_Off();   // 泵只由下面每个状态的进入动作决定，别让它跟着上个状态活下来
        Buzzer_Off(); // 同理。原来 Buzzer_Off() 只有 RETREAT_BEEP 的未变分支在调，
                      // 所以按了 F 再在 2 秒内按别的键，蜂鸣器就一直响 ——
                      // 连急停都停不掉它
        StepMotor_StopAll();
        Servo_StopAllMotions();  // 舵机同理：打断就停在当前角度保持住。
                                 // 不清 is_active 的话它会一直挂着 1，见 Servo.c

        // 记录新状态进入时间
        sys_ctrl.state_timestamp = current_time;

        // 更新 last_state
        sys_ctrl.last_state = sys_ctrl.state;
    switch(sys_ctrl.state) {
    case STATE_WAIT:   
				StepMotor_StopAll();
				Pump_Off();   // 停车必须关泵
		   OLED_ShowString(3, 1, "Wait            ");
					break;
    case STATE_APPROACH:
				OLED_ShowString(3, 1, "Approach        ");
				StepMotor_APPROACH();
					break;
		case STATE_RETREAT:    // 后退
        OLED_Clear();
        OLED_ShowString(3, 1, "Retreat Done    ");
				StepMotor_RETREAT();
//        if(current_time - sys_ctrl.state_timestamp > 30000) {
//            sys_ctrl.state = STATE_RETREAT_BEEP;
//            sys_ctrl.state_timestamp = current_time;
//        }
        break;
		
	   case STATE_YOU:        // 右转（原地顺时针）
        OLED_ShowString(3, 1, "YOU             ");
				StepMotor_You();
        break;
		 

    case STATE_ZUO:        // 左转（原地逆时针）
        OLED_ShowString(3, 1, "ZUO             ");
				StepMotor_Zuo();
        break;

	
		case STATE_MOVE_LEFT:   
				StepMotor_MOVE_LEFT();
        break;
		
		case STATE_MOVE_RIGHT:   
				StepMotor_MOVE_RIGHT();
        break;
		case STATE_RESPRAY:// 二次喷涂：泵开直行一段
		{
		        Pump_On();
		        StepMotor_GoForwardFlat(RESPRAY_PULSE);
		        OLED_ShowString(3, 1, "Respray         ");
		    break;
		}
		case STATE_PAINT:
{// 每次进入画线状态都从头开始
            Pump_On();
            StepMotor_GoForwardFlat(PAINT_LONG_PULSE);
            paint_step = PAINT_EDGE1;
            OLED_ShowString(3, 1, "Paint:1/4       ");
            
    break;
}
		
	  case STATE_TING:       // 强制停止
        Pump_Off();
				StepMotor_StopAll();
		    OLED_ShowString(3, 1, "STOPPED         "); 
        break;
		case STATE_KAI://开水泵
					Pump_On();
					OLED_ShowString(3, 1, "KAI             ");
					break;
		case STATE_GUAN://关水泵
					Pump_Off();
					OLED_ShowString(3, 1, "GUAN            ");
					break;
		case STATE_SELFCHECK://整机自检
					selfcheck_step = 0;
					OLED_ShowString(3, 1, "SelfCheck       ");
					break;
			case STATE_GSERVO://关闭舵机
				
						Servo_StartSmoothMotion(0, Servo_GetAngle(0), 40.0f, 2000); // 舵机1
						Servo_StartSmoothMotion(1, Servo_GetAngle(1), 90.0f, 2000);  // 舵机2
		
        // 等待运动完成

						OLED_ShowString(3, 1, "GSERVO          ");
						break;
			
////关闭舵机（非阻塞启动）
//						Servo_StartSmoothMotion(0, Servo_GetAngle(0), 40.0f, 2000);
//						Servo_StartSmoothMotion(1, Servo_GetAngle(1), 90.0f, 2000);
//						OLED_ShowString(3, 1, "GSERVO          ");
//						break;
			
			case STATE_RETREAT_BEEP://蜂鸣器，结束后到等待状态
						Buzzer_On();
						OLED_Clear();
						OLED_ShowString(3, 1, "FMQ             ");
//        if(current_time - sys_ctrl.state_timestamp > 2000) {
//            Buzzer_Off();
//            sys_ctrl.state = STATE_WAIT;
//        }
        break;
				  default:
                break;
    }
}else {
        // 状态未改变，执行周期性任务（如超时判断）
        switch (sys_ctrl.state) {
            case STATE_GSERVO:
                Servo_UpdateAllMotions();
                if (!Servo_IsMoving(0) && !Servo_IsMoving(1)) {
                    sys_ctrl.state = STATE_WAIT;
                }
                break;
            case STATE_SELFCHECK:
                switch (selfcheck_step) {
                    case 0:   // 打开舵臂
                        Servo_StartSmoothMotion(0, Servo_GetAngle(0), 80.0f, 1500);
                        Servo_StartSmoothMotion(1, Servo_GetAngle(1), 50.0f, 1500);
                        selfcheck_step = 1;
                        break;
                    case 1:   // 等舵臂到位
                        Servo_UpdateAllMotions();
                        if (!Servo_IsMoving(0) && !Servo_IsMoving(1)) {
                            StepMotor_GoForward(SELFCHECK_MOVE_PULSE);
                            selfcheck_step = 2;
                            OLED_ShowString(3, 1, "Check Move      ");
                        }
                        break;
                    case 2:   // 等前进走完
                        if (!StepMotor_IsAnyRunning()) {
                            Servo_StartSmoothMotion(0, Servo_GetAngle(0), 40.0f, 1500);
                            Servo_StartSmoothMotion(1, Servo_GetAngle(1), 90.0f, 1500);
                            selfcheck_step = 3;
                            OLED_ShowString(3, 1, "Check Close     ");
                        }
                        break;
                    case 3:   // 等舵臂关回
                        Servo_UpdateAllMotions();
                        if (!Servo_IsMoving(0) && !Servo_IsMoving(1)) {
                            OLED_ShowString(3, 1, "Check Done      ");
                            sys_ctrl.state = STATE_WAIT;
                        }
                        break;
                    default:
                        sys_ctrl.state = STATE_WAIT;
                        break;
                }
                break;
            case STATE_RETREAT_BEEP:
                if (current_time - sys_ctrl.state_timestamp > 2000) {
                    Buzzer_Off();
                    sys_ctrl.state = STATE_WAIT;
                }
                break;

            case STATE_RESPRAY:
                if (!StepMotor_IsAnyRunning()) {
                    Pump_Off();
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
                            OLED_ShowString(3, 1, "Turn1           ");
                        }
                        break;
                    case PAINT_TURN1:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForwardFlat(PAINT_SHORT_PULSE);
                            paint_step = PAINT_EDGE2;
                            OLED_ShowString(3, 1, "Paint:2/4       ");
                        }
                        break;
                    case PAINT_EDGE2:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            StepMotor_TurnRight(PAINT_TURN_PULSE);
                            paint_step = PAINT_TURN2;
                            OLED_ShowString(3, 1, "Turn2           ");
                        }
                        break;
                    case PAINT_TURN2:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForwardFlat(PAINT_LONG_PULSE);
                            paint_step = PAINT_EDGE3;
                            OLED_ShowString(3, 1, "Paint:3/4       ");
                        }
                        break;
                    case PAINT_EDGE3:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            StepMotor_TurnRight(PAINT_TURN_PULSE);
                            paint_step = PAINT_TURN3;
                            OLED_ShowString(3, 1, "Turn3           ");
                        }
                        break;
                    case PAINT_TURN3:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_On();
                            StepMotor_GoForwardFlat(PAINT_SHORT_PULSE);
                            paint_step = PAINT_EDGE4;
                            OLED_ShowString(3, 1, "Paint:4/4       ");
                        }
                        break;
                    case PAINT_EDGE4:
                        if (!StepMotor_IsAnyRunning()) {
                            Pump_Off();
                            paint_step = PAINT_DONE;
                            sys_ctrl.state = STATE_WAIT;
                            OLED_ShowString(3, 1, "Paint Done      ");
                        }
                        break;
                    default:
                        break;
               }
                break;
            }
            // 遥控看门狗：点动指令靠重复发送续命，超时说明蓝牙断了或人松手了。
            // 先柔和减速（StopJog 幂等），减速跑完再回等待 —— 直接切状态的话，
            // 状态退出时的 StopAll 会把它硬停掉，白减速一场。
            case STATE_APPROACH:  case STATE_RETREAT:
            case STATE_YOU:       case STATE_ZUO:
            case STATE_MOVE_LEFT: case STATE_MOVE_RIGHT:
                if (current_time - sys_ctrl.state_timestamp > JOG_TIMEOUT_MS) {
                    StepMotor_StopJog();
                }
                if (!StepMotor_IsAnyRunning()) {
                    sys_ctrl.state = STATE_WAIT;
                }
                break;
            default:
                break;
        }
    }
}
