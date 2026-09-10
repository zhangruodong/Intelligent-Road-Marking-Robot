import os

os.environ.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
os.environ.pop("QT_PLUGIN_PATH", None)
import sys
from PyQt5.QtWidgets import QApplication
from core.serial_manager import SerialManager
from core.protocol import Protocol
from ui.main_window import MainWindow

# 用 /dev/serial0：它是个 udev 符号链接，永远指向"当前接在排针上的那个串口"，
# 所以底下到底是哪个 UART 都不用改代码。
#
# 这台是树莓派 4B，两种情况都会碰到：
#   默认（蓝牙开着）  -> 排针 = /dev/ttyS0 = mini-UART，/dev/ttyAMA0 归蓝牙
#   dtoverlay=disable-bt -> 排针 = /dev/ttyAMA0 = PL011，蓝牙没了
# 所以别把默认写死成 ttyAMA0 —— 在没改 config.txt 的 4B 上那是蓝牙口，收不到东西。
#
# 另外 4B 开蓝牙的话排针走的是 mini-UART，它的波特率分频是从 VPU 核心时钟推的，
# 核心时钟一变（负载/turbo）实际波特率就跟着漂，9600 下会开始丢字节、指令错乱。
# 上位机还跑着 OpenCV + PyQt5，这事儿真会发生。要根治就在 /boot/firmware/config.txt
# 加一行 dtoverlay=disable-bt（排针换成 PL011，/dev/serial0 自动指过去，代码不用动）。
# 见 README 的注意事项。
#
# 临时换口不用改代码：MARKING_PORT=/dev/ttyUSB0 ./start.sh
PORT = os.environ.get("MARKING_PORT", "/dev/serial0")

def main():
    app = QApplication(sys.argv)
    serial_mgr = SerialManager(PORT, 9600)
    protocol = Protocol()
    window = MainWindow(serial_mgr, protocol)
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()