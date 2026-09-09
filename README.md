# 智能道路标线机器人（Intelligent-Road-Marking-Robot）

基于 STM32F103C8T6 的智能道路标线机器人，采用**上下位机**架构：树莓派上位机（Python / PyQt5 + OpenCV 视觉）负责检测与决策，STM32 下位机负责四轮步进底盘、舵机喷臂、水泵喷漆等底层执行。

## 特性

**下位机（marking-firmware，STM32）**

- **非阻塞状态机**：所有动作由 1ms 时间戳推进，主循环始终能响应串口指令。
- **四轮步进电机底盘**：TIM3 四路 PWM 输出脉冲 + 方向脚，直线运动带梯形加减速，旋转 / 平移为固定速度。
- **舵机平滑运动**：TIM4 双路 PWM（20ms 周期），三次 Hermite（smoothstep）缓起缓停。
- **自动标线流程**：`P` 指令一键画出完整矩形（长 5.3m × 宽 2.5m），水泵随行喷漆。
- **3 路串口**：USART1 / USART2 / USART3 均 9600 8N1，中断接收。
- **OLED 实时状态显示**：软件 I2C 屏显示当前状态。

**上位机（marking-host，树莓派）**

- **PyQt5 控制面板**：串口收发日志 + 舵机测试 + 自动回充控制 + 整机自检。
- **AprilTag 视觉自动回充**：OpenCV ArUco 检测充电桩标签（36h11，ID=0），舵机扫描 + 车身旋转 + 退桩对接状态机。
- **黄色标线检测**：HSV 提黄 + Canny + 霍夫直线，识别地面车道线（独立脚本）。
- **11 字节帧串口协议**：`FD | CMD | 数据 | BCC | DF`，带异或校验。

## 硬件平台

| 项目 | 参数 |
|---|---|
| 下位机 MCU | STM32F103C8T6（LQFP48，64KB Flash，72MHz） |
| 上位机 | 树莓派 5B + Python3 + PyQt5 + OpenCV |
| 下位机开发环境 | Keil MDK（ARMCC） |
| 串口 | USART1 / USART2 / USART3，9600 8N1 |

## 目录结构

```
Intelligent-Road-Marking-Robot/
├── marking-host/            # 上位机（树莓派 Python + 视觉）
│   ├── main.py              # 程序入口
│   ├── core/                # protocol / serial_manager / dock_controller / tag_detector
│   ├── ui/                  # main_window / camera_widget
│   ├── camera_test.py       # 黄色标线检测（独立脚本）
│   ├── serial_test.py       # 串口收发测试
│   ├── install.sh           # 树莓派部署脚本
│   └── start.sh             # 桌面启动脚本
└── marking-firmware/        # 下位机（STM32 Keil 工程）
    ├── Hardware/            # 外设驱动（电机/舵机/水泵蜂鸣器/OLED/串口/状态机/自检）
    ├── User/                # main.c、stm32f10x_it.c、stm32f10x_conf.h
    ├── System/              # Delay 延时
    ├── Library/             # STM32F10x 标准外设库
    ├── Start/               # 启动文件、system_stm32f10x、core_cm3
    └── Project.uvprojx      # Keil 工程
```

**下位机核心逻辑在 [marking-firmware/Hardware/system.c](marking-firmware/Hardware/system.c)**：`System_StateMachine()` 是主状态机，`ProcessCommand()` 处理串口指令。
**上位机核心在 [marking-host/main.py](marking-host/main.py)** 与 [marking-host/core/](marking-host/core/)。

## 接线表（下位机）

| 外设 | 引脚 | 类型 | 说明 |
|---|---|---|---|
| 步进电机 1（前左轮） | PA8 = DIR，PA6 = PUL | 步进 | PUL = TIM3_CH1 |
| 步进电机 2（前右轮） | PA12 = DIR，PA7 = PUL | 步进 | PUL = TIM3_CH2 |
| 步进电机 3（后左轮） | PA11 = DIR，PB0 = PUL | 步进 | PUL = TIM3_CH3 |
| 步进电机 4（后右轮） | PB12 = DIR，PB1 = PUL | 步进 | PUL = TIM3_CH4 |
| 舵机 1 | PB6 | PWM（TIM4_CH1） | 20ms 周期，0~180° → 500~2500us |
| 舵机 2 | PB7 | PWM（TIM4_CH2） | 20ms 周期，0~180° → 500~2500us |
| 水泵 | PB13 | 输出 | 高电平开 |
| 蜂鸣器 | PB14 | 输出 | 低电平响 |
| OLED 屏 | PB8 = SCL，PB9 = SDA | 软件 I2C | 从机地址 0x78 |
| 心跳指示灯 | PB15 | 输出 | TIM1 1ms 中断，500ms 翻转 |
| 串口 USART1 | PA9 = TX，PA10 = RX | UART | 9600 8N1 |
| 串口 USART2 | PA2 = TX，PA3 = RX | UART | 9600 8N1 |
| 串口 USART3 | PB10 = TX，PB11 = RX | UART | 9600 8N1 |

## 串口指令（下位机）

下位机接收**单字节 ASCII 字符**指令：

| 指令 | 功能 |
|---|---|
| `W` | 等待，停止所有电机 |
| `U` | 前进 |
| `R` | 后退 |
| `Y` | 原地右转（顺时针） |
| `Z` | 原地左转（逆时针） |
| `A` | 左移（平移） |
| `X` | 右移（平移） |
| `T` | 强制停止所有执行器 |
| `P` | 自动标线（长 5.3m × 宽 2.5m 完整矩形） |
| `K` | 开水泵 |
| `G` | 关水泵 |
| `D` | 打开舵臂 |
| `J` | 关闭舵臂 |
| `F` | 蜂鸣提示（后退提示音） |

## 上位机通信协议

上位机发送 **11 字节帧**：

```
FD | CMD | 保留 | 4 字节数据 | BCC | DF
```

- `FD` 帧头，`DF` 帧尾，`BCC` 为前 9 字节异或校验。
- `CMD = 0`（CMD_VEL）：速度指令，数据为 Vx / Vy / Vz（mm/s，int16）。
- `CMD = 4`（CMD_SERVO）：舵机脉宽，数据为 pulse_us（500~2500）。

## 编译

**下位机**

1. 用 Keil MDK 打开 `marking-firmware/Project.uvprojx`。
2. 确认目标器件为 STM32F103C8（64KB Flash）。
3. 编译（Build），通过 ST-Link / J-Link 下载。

**上位机**

```bash
cd marking-host
python3 -m venv venv && source venv/bin/activate
pip install pyqt5 pyserial opencv-python opencv-contrib-python numpy
python3 main.py
```

## 注意事项

- **通信协议尚未统一**：下位机当前是单字符 ASCII 指令，上位机当前是 11 字节帧，且波特率不一致（上位机 115200，下位机 9600）。联调前需二选一对齐（改下位机加帧解析，或改上位机发单字符）。
- **PA13（SWDIO）/ PA14（SWCLK）保留给 SWD 下载调试**，不要接外设。
- 步进电机方向宏（`M1_FWD` ~ `M4_REV`）已按实车标定，若换电机 / 接线后方向反转，在 `StepMotor_New.h` 里取反即可。
