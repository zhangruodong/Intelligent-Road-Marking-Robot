import cv2
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QTextEdit, QLabel,
    QStackedLayout, QSpacerItem, QSizePolicy
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QTextCursor
from ui.camera_widget import CameraWidget


class MainWindow(QWidget):

    def __init__(self, serial_mgr, protocol, dock_ctrl, tag_detector):
        super().__init__()
        self.serial_mgr = serial_mgr
        self.proto = protocol
        self.dock = dock_ctrl
        self.tag = tag_detector

        self.init_ui()
        self.serial_mgr.open()

        self.dock.on_tag_found = self._on_tag_found
        self.dock.on_done = self._on_dock_done

        self.status_timer = QTimer()
        self.status_timer.timeout.connect(self._update_status)
        self.status_timer.start(200)

    # ===================== UI =====================
    def init_ui(self):
        self.resize(1000, 500)
        self.setWindowTitle("V650 自动回充控制面板")
        self.stack_layout = QStackedLayout()
        self.setLayout(self.stack_layout)

        # ——— 主界面 ———
        self.main_widget = QWidget()
        main_layout = QHBoxLayout()
        self.main_widget.setLayout(main_layout)

        # ===== 左侧 =====
        left_layout = QVBoxLayout()
        left_layout.setAlignment(Qt.AlignTop)

        self.status_label = QLabel("状态:\n就绪")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("font-size:32px; font-weight:bold;")
        left_layout.addSpacerItem(QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Fixed))
        left_layout.addWidget(self.status_label)
        left_layout.addStretch()

        self.btn_dock = QPushButton("开始自动回充")
        self.btn_servo_test = QPushButton("舵机测试(右)")
        self.btn_servo_left = QPushButton("舵机测试(左)")
        self.btn_servo_mid = QPushButton("舵机回中")
        self.btn_stop = QPushButton("紧急停止")

        self.control_buttons = [
            self.btn_dock, self.btn_servo_test,
            self.btn_servo_left, self.btn_servo_mid, self.btn_stop
        ]

        for btn in self.control_buttons:
            btn.setMinimumHeight(40)
            btn.setStyleSheet("font-size:20px;")
            left_layout.addWidget(btn)

        self.btn_dock.clicked.connect(self.start_dock)
        self.btn_servo_test.clicked.connect(
            lambda: self.serial_mgr.send(self.proto.build_servo_frame(2500)))
        self.btn_servo_left.clicked.connect(
            lambda: self.serial_mgr.send(self.proto.build_servo_frame(500)))
        self.btn_servo_mid.clicked.connect(
            lambda: self.serial_mgr.send(self.proto.build_servo_frame(1500)))
        self.btn_stop.clicked.connect(self.stop_dock)

        self.btn_settings = QPushButton("设置")
        self.btn_settings.setMinimumHeight(40)
        self.btn_settings.setStyleSheet("font-size:20px;")
        self.btn_settings.clicked.connect(self.show_settings)
        left_layout.addWidget(self.btn_settings)

        main_layout.addLayout(left_layout, 1)

        # ===== 中间（发送日志） =====
        center_layout = QVBoxLayout()
        self.send_log_label = QLabel("发送日志")
        self.send_log = QTextEdit()
        self.send_log.setReadOnly(True)
        center_layout.addWidget(self.send_log_label)
        center_layout.addWidget(self.send_log, stretch=1)

        self.btn_clear_send = QPushButton("清空发送日志")
        self.btn_clear_send.clicked.connect(self.clear_send_log)
        center_layout.addWidget(self.btn_clear_send)

        main_layout.addLayout(center_layout, 1)

        # ===== 右侧（接收日志） =====
        right_layout = QVBoxLayout()
        self.recv_log_label = QLabel("接收日志")
        self.recv_log = QTextEdit()
        self.recv_log.setReadOnly(True)
        right_layout.addWidget(self.recv_log_label)
        right_layout.addWidget(self.recv_log, stretch=1)

        self.btn_clear_recv = QPushButton("清空接收日志")
        self.btn_clear_recv.clicked.connect(self.clear_recv_log)
        right_layout.addWidget(self.btn_clear_recv)

        main_layout.addLayout(right_layout, 1)
        self.stack_layout.addWidget(self.main_widget)

        # ——— 设置界面 ———
        self.settings_widget = QWidget()
        settings_layout = QVBoxLayout()
        self.settings_widget.setLayout(settings_layout)

        self.settings_title = QLabel("设置")
        self.settings_title.setAlignment(Qt.AlignCenter)
        self.settings_title.setStyleSheet("font-size:32px; font-weight:bold;")
        settings_layout.addWidget(self.settings_title)

        self.btn_back_settings = QPushButton("返回主界面")
        self.btn_back_settings.clicked.connect(self.show_main)
        settings_layout.addWidget(self.btn_back_settings)

        self.btn_camera = QPushButton("打开摄像头")
        self.btn_camera.setMinimumHeight(40)
        self.btn_camera.setStyleSheet("font-size:18px;")
        self.btn_camera.clicked.connect(self.open_camera)
        settings_layout.addWidget(self.btn_camera)

        self.stack_layout.addWidget(self.settings_widget)

    # ===================== 页面切换 =====================
    def show_settings(self):
        self.setWindowTitle("设置")
        self.stack_layout.setCurrentWidget(self.settings_widget)

    def show_main(self):
        self.setWindowTitle("V650 自动回充控制面板")
        self.stack_layout.setCurrentWidget(self.main_widget)

    # ===================== 日志 =====================
    def clear_send_log(self):
        self.send_log.clear()

    def clear_recv_log(self):
        self.recv_log.clear()

    def log_send(self, data: bytes, label=""):
        self.send_log.append(f"{data.hex()} ({label})")
        self.send_log.moveCursor(QTextCursor.End)

    def log_recv(self, text: str):
        self.recv_log.append(text)
        self.recv_log.moveCursor(QTextCursor.End)

    # ===================== 回充 =====================
    def start_dock(self):
        self.status_label.setText("状态:\n扫描中...")
        self.dock.start()

    def stop_dock(self):
        self.dock.stop()
        self.serial_mgr.send(self.proto.build_vel_frame(0, 0, 0))
        self.status_label.setText("状态:\n已停止")

    def _on_tag_found(self):
        self.status_label.setText("状态:\n旋转对准中...")

    def _on_dock_done(self):
        self.status_label.setText("状态:\n对接完成")

    def _update_status(self):
        states = ['待机', '扫描', '旋转', '退桩', '完成']
        self.log_recv(f"[状态] 舵机:{self.dock.servo_pos}us  {states[self.dock.state]} "
                      f"标签:{'找到' if self.dock.tag_seen else '未检测'}")

    # ===================== 摄像头 =====================

    # ===================== 整机自检 =====================
    def start_self_test(self):
        self.status_label.setText("状态:
自检中...")
        self.selftest_step = 0
        self._do_selftest()

    def _selftest_step(self):
        self._do_selftest()

    def _do_selftest(self):
        self.selftest_timer.stop()
        step = self.selftest_step

        if step == 0:
            # 前进 2 秒
            self.log_send(b"", "自检: 前进")
            self.serial_mgr.send(self.proto.build_vel_frame(0.2, 0, 0))
            self.selftest_timer.start(2000)
        elif step == 1:
            # 后退 2 秒
            self.log_send(b"", "自检: 后退")
            self.serial_mgr.send(self.proto.build_vel_frame(-0.2, 0, 0))
            self.selftest_timer.start(2000)
        elif step == 2:
            # 停止 + 舵机右转
            self.log_send(b"", "自检: 停止 + 舵机右")
            self.serial_mgr.send(self.proto.build_vel_frame(0, 0, 0))
            self.serial_mgr.send(self.proto.build_servo_frame(2500))
            self.selftest_timer.start(1000)
        elif step == 3:
            # 舵机左转
            self.log_send(b"", "自检: 舵机左")
            self.serial_mgr.send(self.proto.build_servo_frame(500))
            self.selftest_timer.start(1000)
        elif step == 4:
            # 舵机回中
            self.log_send(b"", "自检: 舵机回中")
            self.serial_mgr.send(self.proto.build_servo_frame(1500))
            self.selftest_timer.start(500)
        elif step == 5:
            self.log_send(b"", "自检: 完成")
            self.status_label.setText("状态:
自检完成")
            self.selftest_step = -1
            return

        self.selftest_step += 1

    def open_camera(self):
        self.camera_window = QWidget()
        self.camera_window.setWindowTitle("摄像头画面")
        self.camera_window.resize(800, 600)
        layout = QVBoxLayout()
        self.camera_window.setLayout(layout)
        self.camera_widget = CameraWidget()
        layout.addWidget(self.camera_widget)
        self.camera_widget.start_camera()
        self.camera_widget.tag_detector = self.tag
        self.camera_widget.on_tag_seen = lambda: self.dock.tag_detected()
        self.camera_window.show()
