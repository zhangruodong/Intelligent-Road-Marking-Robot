# 智能道路标线机器人（Intelligent-Road-Marking-Robot）

基于 STM32F103C8T6 的智能道路标线机器人，采用**上下位机**架构：树莓派上位机（Python / PyQt5 + OpenCV 视觉）负责检测与决策，STM32 下位机负责四轮步进底盘、舵机喷臂、水泵喷漆等底层执行。

## 特性

**下位机（marking-firmware，STM32）**

- **非阻塞状态机**：所有动作由 1ms 时间戳推进，主循环始终能响应串口指令。
- **四轮步进电机底盘**：TIM3 四路 PWM 输出脉冲 + 方向脚，直线运动带梯形加减速，旋转 / 平移为固定速度。
- **舵机平滑运动**：TIM4 双路 PWM（20ms 周期），三次 Hermite（smoothstep）缓起缓停。
- **自动标线流程**：`P` 指令一键画出完整矩形（长 5.3m × 宽 2.5m），水泵随行喷漆。
- **2 路串口**：USART1（蓝牙手机）/ USART2（树莓派上位机），均 9600 8N1，中断接收。
- **OLED 实时状态显示**：软件 I2C 屏显示当前状态。

**上位机（marking-host，树莓派）**

- **PyQt5 控制面板**：画车位控制（标准 / 路沿）+ 二次喷涂 + 整机自检 + 水泵清洗 + 紧急停止，串口收发日志。
- **单字节串口协议**：每条指令一个 ASCII 字符（`P`/`D`/`K`/`G`/`N`/`C`/`W`/`T`），9600 8N1。
- **摄像头画面弹窗**：二次喷涂 / 画路沿时弹出实时画面。
- **黄色标线检测（闭环）**：HSV 提黄 → 形态学去噪 → 闭运算补裂缝 → 轮廓长宽比筛线状 →
  PCA 主方向定矩形 → 在矩形内量**原始黄像素**的覆盖率判破损（阈值集中在
  `core/line_quality.py`，不依赖 Qt，可单独跑 `vision_test.py`）。
  破损就按固定周期往下位机发一次 `C` 补喷一段。
  (`camera_test.py` 里另有一套 HSV + Canny + 霍夫的调试用可视化，不参与闭环。)

## 硬件平台

| 项目 | 参数 |
|---|---|
| 下位机 MCU | STM32F103C8T6（LQFP48，64KB Flash，72MHz） |
| 上位机 | 树莓派 4B + Python3 + PyQt5 + OpenCV |
| 下位机开发环境 | Keil MDK（ARMCC） |
| 串口 | USART1（蓝牙）/ USART2（树莓派），9600 8N1 |

## 目录结构

```
Intelligent-Road-Marking-Robot/
├── marking-host/            # 上位机（树莓派 Python + 视觉）
│   ├── main.py              # 程序入口
│   ├── core/                # protocol（指令表）/ serial_manager / line_quality（判据阈值）
│   ├── ui/                  # main_window / camera_widget
│   ├── protocol_test.py     # 上下位机指令集对账
│   ├── vision_test.py       # 破损判据
│   ├── camera_widget_test.py# C 闭环 / 限流 / 取景开关
│   ├── camera_test.py       # 黄色标线检测（独立脚本，调试用可视化）
│   ├── serial_test.py       # 串口收发测试
│   ├── install.sh           # 树莓派部署脚本
│   └── start.sh             # 桌面启动脚本
└── marking-firmware/        # 下位机（STM32 Keil 工程）
    ├── Hardware/            # 外设驱动（电机/舵机/水泵蜂鸣器/OLED/串口/状态机）
    ├── User/                # main.c、stm32f10x_it.c、stm32f10x_conf.h
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
| 串口 USART1（蓝牙手机） | PA9 = TX，PA10 = RX | UART | 9600 8N1，RX 上拉输入 |
| 串口 USART2（树莓派） | PA2 = TX，PA3 = RX | UART | 9600 8N1，RX 上拉输入 |

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
| `T` | 强制停止所有执行器（状态停在 TING，`C` 等会自己动作的指令随之失效） |
| `P` | 画车位（标准，长 5.3m × 宽 2.5m 完整矩形，水泵随行） |
| `C` | 二次喷涂补线，泵开直行一段后自停。**只在 `W` 状态被接受** —— 已经在喷就不重进、不打断正在跑的活、急停后进不来 |
| `K` | 开水泵 |
| `G` | 关水泵 |
| `D` | 整机自检（开臂 → 前进 → 合臂） |
| `J` | 关闭舵臂（舵机平滑回到 40° / 90°） |
| `F` | 蜂鸣提示（后退提示音） |
| `+` / `-` | 蓝牙专用调速，上位机发来忽略。运动中的按键只改目标速度，斜坡自己走过去 |

需要单独说明的两点：

- **`N`（画车位（路沿））下位机尚未实现**，按下去是个空操作。上位机这边按钮和指令表都有，
  缺口由 `protocol_test.py::test_not_implemented_is_still_true` 盯着 —— 固件补上它，测试就会报。
- **上电首次合臂**：`Hardware_Init` 会先 `Servo_SetAngle(40)` / `Servo2_SetAngle(90)`，
  因为 DPWM 初始 `TIM_Pulse = 0`（占空比 0%），不写这两句舵机在收到第一条 `J`/`D`
  之前完全没有信号，舵臂会被外力带着乱动。

## 上位机通信协议

上位机发送**单字节 ASCII 指令**（9600 8N1）：

| 指令 | 功能 |
|---|---|
| `P` | 画车位（标准） |
| `D` | 整机自检 |
| `K` | 水泵清洗 |
| `G` | 关水泵 |
| `N` | 画车位（路沿）—— 下位机未实现 |
| `C` | 二次喷涂（检测到破损自动发，每 2s 一条） |
| `W` | 停止复位 |
| `T` | 紧急停止 |

按钮表只定义在 [marking-host/core/protocol.py](marking-host/core/protocol.py) 的 `PANEL_BUTTONS`
里一处，`main_window.py` 和摄像头弹窗判断都从那儿取 —— 标签写两遍就会出现
"某个按钮悄悄失效"（`N`/`C` 当初就是这么漏的）。

下位机回传同为单字节（收到什么原样回显什么），上位机按同表解析状态。

### `C` 的闭环怎么走

`二次喷涂` 会开一个带串口的摄像头窗口（`CameraWidget(serial_mgr, auto_paint=True)`），
检测到破损就调 `maybe_send_respray()` 发 `C`。几个刻意的设计：

- **一次只发一条、间隔 ≥ `SPRAY_INTERVAL_S`（2s，略大于下位机 `RESPRAY_PULSE` 跑完的时间）**，
  全速重发没有意义，下位机那边也只在 `W` 状态接受。
- **按下面板上任何别的键就撤下自动喷涂**（`send_button` 里把 `auto_paint` 置回 `False`），
  不然按了"停止复位"或"关水泵"两秒后相机又自己开喷。要接着喷就再按一次"二次喷涂"。
- **只是取景的"画车位（路沿）"窗口 `auto_paint=False`，一个字节都不发。**

## 编译

**下位机**

1. 用 Keil MDK 打开 `marking-firmware/Project.uvprojx`。
2. 确认目标器件为 STM32F103C8（64KB Flash）。
3. 编译（Build），通过 ST-Link / J-Link 下载。

**上位机**

在树莓派上克隆仓库，然后跑安装脚本：

```bash
cd marking-host
./install.sh          # 装 python3-pyqt5 / python3-serial / python3-opencv，并在桌面建启动图标
python3 main.py
```

**用 apt 装，不要用 pip + venv。** `pip install pyqt5` 在 aarch64 上要现场编译
Qt，又慢又容易失败；apt 的 `python3-pyqt5` 是编好的。三个 apt 包按系统 python3
解析，所以直接 `python3 main.py` 就行，不用激活什么环境。

> 仓库里如果有个 `venv/` 目录，那是早先试过的，已经废弃 ——
> 里面只有 cv2 和 numpy（`include-system-site-packages = false`，连 apt 装的
> PyQt5 都看不见），激活它反而会让程序起不来。可以删掉。

## 注意事项

- **上下位机的指令集对齐由测试守着**：`python3 protocol_test.py` 直接从固件
  `system.c` 抠 `case '...'` 和面板按钮表对账，加指令忘了同步会当场报。唯一
  已知的缺口是 `N`，写在该测试的 `NOT_IMPLEMENTED` 里。
- **上位机串口默认 `/dev/serial0`**（`main.py` / `serial_manager.py` / `serial_test.py`）。
  `serial0` 是 udev 符号链接，永远指向"当前接在排针上的那个串口"，所以底下换 UART
  不用改代码。临时换口（比如 USB 转串口）用环境变量：`MARKING_PORT=/dev/ttyUSB0 ./start.sh`。
- **树莓派 4B 的排针串口是哪个 UART 取决于配置**，别写死成 `ttyAMA0`：

  | `config.txt` | 排针（GPIO14/15） | 蓝牙用 |
  |---|---|---|
  | 默认（蓝牙开着） | `/dev/ttyS0`（mini-UART） | `/dev/ttyAMA0`（PL011） |
  | `dtoverlay=disable-bt` | `/dev/ttyAMA0`（PL011） | 无 |

  默认配置下排针是 **mini-UART**，它的波特率分频是从 VPU 核心时钟推出来的，核心
  时钟一变（负载、turbo）实际波特率就跟着漂 —— 9600 下会开始丢字节、指令错乱。
  上位机同时跑着 OpenCV + PyQt5，这事儿真会发生。**推荐在
  `/boot/firmware/config.txt` 加一行 `dtoverlay=disable-bt`**：排针换成时钟独立的
  PL011，蓝牙功能没了（本项目不用蓝牙，STM32 那侧 USART1 是给手机用的），
  `/dev/serial0` 会自动指过去，代码一行都不用改。
- **串口控制台必须关掉**（`raspi-config` → Interface → Serial Port → login shell 选
  No、hardware 选 Yes），否则内核日志会从 TX 打到 STM32 那边去。
  这一项和上面的 `dtoverlay=disable-bt` **都要手动做、都要重启**，`install.sh`
  不碰系统配置 —— 它只装包、把当前用户加进 `dialout` 组、生成桌面图标。
- **步进是开环的**，没有编码器，丢步不可恢复。所以运动起停都走梯形斜坡
  （`RAMP_SOFT_ARR` 1800 → `speed_arr`，每 `RAMP_SOFT_STEP_PULSE` 个脉冲走
  `RAMP_SOFT_STEP_ARR`）；调试时别绕过斜坡，也别在加速段中间硬改频率。
  真要把累积误差消掉，只能靠加限位/回零开关重新对基准。
- **舵机角度钳在 0~180°**（`Servo.c` 的 `Servo_ClampAngle`）。越界的指令会
  停在端点角度保持住，而不是让 PWM 比较值出界：出界的比较值要么算出 0（恒低），
  要么超过 ARR（恒高），两种都没有 500~2500us 的有效脉宽 —— 舵机收不到位置信号
  就没有保持力矩，机械臂会靠重力耷下来。
- **PA13（SWDIO）/ PA14（SWCLK）保留给 SWD 下载调试**，不要接外设。
- 步进电机方向宏（`M1_FWD` ~ `M4_REV`）已按实车标定，若换电机 / 接线后方向反转，在 `StepMotor_New.h` 里取反即可。

## 许可

本项目作者写的代码（`marking-firmware/Hardware/`、`marking-firmware/User/`、
`marking-host/`）按顶层 [LICENSE](LICENSE) 的 **MIT** 授权。

但仓库里还带着两坨第三方代码，**不在 MIT 覆盖范围内**：

| 目录 | 内容 | 归属 |
|---|---|---|
| `marking-firmware/Library/` | STM32F10x 标准外设库 V3.5.0（46 个文件） | © 2011 STMicroelectronics |
| `marking-firmware/Start/` | ARM CMSIS `core_cm3` + ST 启动文件 | © 2009 ARM Limited / © 2011 STMicroelectronics |

ST 那份是 ST 自家的协议（不是 OSI 开源协议），ARM 那份的授权也比 MIT 窄。
所以**这个仓库整体不能简单说成 "MIT 许可"** —— 详见表
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

那两个目录里的版权声明请**不要删改**，删了就失去了归属信息。
