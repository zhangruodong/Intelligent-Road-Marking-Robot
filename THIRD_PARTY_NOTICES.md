# 第三方代码

本仓库里有一部分代码不是本项目作者写的，**不在顶层 MIT 协议的覆盖范围内**，
各自沿用原来的条款。下面把它们列清楚。

如果你要复用本仓库，请注意：**顶层那个 MIT 只覆盖作者自己写的代码**
（`marking-firmware/Hardware/`、`marking-firmware/User/`、`marking-host/`），
`marking-firmware/Library/` 和 `marking-firmware/Start/` 不归它管。

## 一、STM32F10x 标准外设库 V3.5.0

| | |
|---|---|
| 位置 | `marking-firmware/Library/`（46 个文件） |
|  | `marking-firmware/Start/startup_stm32f10x_*.s`（8 个） |
|  | `marking-firmware/Start/stm32f10x.h` |
|  | `marking-firmware/Start/system_stm32f10x.c`、`.h` |
| 版权 | © 2011 STMicroelectronics，MCD Application Team |
| 版本 | V3.5.0（2011-03-11） |

这些文件是 ST 官方发布包里的原样代码。它们的抬头只写了版权和一段免责声明
（`THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY ...`），**没有写明适用哪份
协议** —— ST 从 V3.6.1（2012）起才在抬头里明确标注
`Licensed under MCD-ST Liberty SW License Agreement V2`。

ST 这份协议是 ST 自己拟的，不是 OSI 认可的开源协议，**与本仓库的 MIT 不兼容**。
本项目只是把 ST 的库原样放在这里供编译使用，无权把它转授成 MIT，也没有修改
ST 的版权声明。要确认具体条款请以 ST 官方发布的协议文本为准。

> 这些文件的抬头请**不要删改**，删了就失去了归属信息。

## 二、ARM CMSIS Cortex-M3 内核支持文件

| | |
|---|---|
| 位置 | `marking-firmware/Start/core_cm3.c`、`core_cm3.h` |
| 版权 | © 2009 ARM Limited |
| 版本 | V1.30（2009-10-30） |

ARM 的条款比 MIT 窄：允许"在支持 Cortex-M 处理器的开发工具内自由分发"，
并不是任意使用。同样不在顶层 MIT 的覆盖范围内。文件抬头里的 ARM 声明请保留。

## 三、你自己的代码

`marking-firmware/Hardware/`、`marking-firmware/User/`、`marking-host/`、
README、以及三个测试脚本 —— 这些是作者写的，按顶层 `LICENSE` 的 MIT 授权。

---

## 想彻底避开这两个限制

把 `marking-firmware/Library/` 和 `marking-firmware/Start/` 换成 ST 现在的
**CubeF1** 库（BSD-3-Clause，OSI 认可）就能去掉上面两条，整个仓库就能干净地
只用 MIT。但那是移植工作量（标准外设库和 HAL 的 API 完全不同，`Hardware/`
下面每个驱动都要重写），不是许可问题本身。当前这个项目没必要动。

另一个折中：把这两个目录从仓库里拿掉，改成编译前自己去 ST 下载。代价是别人
clone 下来不能直接编译，和"点一下就能下到全部代码"冲突。
