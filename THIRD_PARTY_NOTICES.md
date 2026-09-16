# 第三方代码

本项目自己写的代码（`marking-firmware/Hardware/`、`marking-firmware/User/`、
`marking-host/`）按顶层 [LICENSE](LICENSE) 的 **MIT** 授权。

下面这两份是随仓库一起分发的**第三方代码**，不属于本项目作者，也不在 MIT 的
授权范围内，各自沿用原来的条款。两份都原样保留，版权声明未作改动。

## 一、STM32F10x 标准外设库 V3.5.0

| | |
|---|---|
| 位置 | `marking-firmware/Library/`（46 个文件） |
|  | `marking-firmware/Start/startup_stm32f10x_*.s`（8 个） |
|  | `marking-firmware/Start/stm32f10x.h` |
|  | `marking-firmware/Start/system_stm32f10x.c`、`.h` |
| 版权 | © 2011 STMicroelectronics，MCD Application Team |
| 版本 | V3.5.0（2011-03-11） |

ST 官方发布包里的原样代码，供本工程编译使用。抬头只写了版权和一段免责声明
（`THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY ...`），**没有写明适用哪份
协议** —— ST 从 V3.6.1（2012）起才在抬头里明确标注
`Licensed under MCD-ST Liberty SW License Agreement V2`。

这是 ST 自己拟的条款，不是 OSI 认可的开源协议。本项目只是原样分发，**无权把
它转授成 MIT**。具体条款以 ST 官方发布的协议文本为准。

## 二、ARM CMSIS Cortex-M3 内核支持文件

| | |
|---|---|
| 位置 | `marking-firmware/Start/core_cm3.c`、`core_cm3.h` |
| 版权 | © 2009 ARM Limited |
| 版本 | V1.30（2009-10-30） |

ARM 的条款是"在支持 Cortex-M 处理器的开发工具内自由分发"，比 MIT 窄，同样
不在 MIT 的覆盖范围内。

---

上面两个目录里的版权声明请**不要删改**，删了就失去了归属信息。
