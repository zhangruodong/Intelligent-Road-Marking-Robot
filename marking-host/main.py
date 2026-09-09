import os
os.environ.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
os.environ.pop("QT_PLUGIN_PATH", None)

import sys
from PyQt5.QtWidgets import QApplication
from core.serial_manager import SerialManager
from core.protocol import Protocol
from core.dock_controller import DockController
from core.tag_detector import TagDetector
from ui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)

    # 串口: 树莓派 5B UART3 → V650 STM32, 115200
    serial_mgr = SerialManager("/dev/v650", 115200)
    protocol = Protocol()

    # AprilTag 检测器
    tag_detector = TagDetector()

    # 自动回充控制器
    dock_ctrl = DockController(protocol)

    # GUI
    window = MainWindow(serial_mgr, protocol, dock_ctrl, tag_detector)
    window.show()

    # 关联串口发送
    dock_ctrl.on_servo_cmd = lambda pulse: serial_mgr.send(
        protocol.build_servo_frame(pulse)
    )
    dock_ctrl.on_vel_cmd = lambda vx, vy, vz: serial_mgr.send(
        protocol.build_vel_frame(vx, vy, vz)
    )

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
