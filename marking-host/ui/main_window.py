import cv2
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QTextEdit, QLabel,
    QComboBox, QCheckBox, QSlider, QStackedLayout, QSpacerItem, QSizePolicy
)
from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QTextCursor
from ui.camera_widget import CameraWidget

class MainWindow(QWidget):
    append_send_signal = pyqtSignal(str)
    append_recv_signal = pyqtSignal(object)

    def __init__(self, serial_mgr, protocol):
        super().__init__()
        self.serial_mgr = serial_mgr
        self.protocol = protocol

        self.append_send_signal.connect(self._append_send)
        self.append_recv_signal.connect(self._append_recv)

        self.init_ui()
        #self.serial_mgr.callback = self.on_data_received
        self.serial_mgr.data_received.connect(
            self.on_data_received
        )
        self.serial_mgr.open()

    def init_ui(self):
        self.resize(1000, 500)
        self.setWindowTitle("道路画线机控制面板")
        self.stack_layout = QStackedLayout()
        self.setLayout(self.stack_layout)

        # ---------------- 主界面 ----------------
        self.main_widget = QWidget()
        main_layout = QHBoxLayout()
        self.main_widget.setLayout(main_layout)

        # 左侧
        left_layout = QVBoxLayout()
        left_layout.setAlignment(Qt.AlignTop)

        self.status_label = QLabel("状态:\nwait")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("font-size:32px; font-weight:bold;")
        left_layout.addSpacerItem(QSpacerItem(20, 40, QSizePolicy.Minimum, QSizePolicy.Fixed))
        left_layout.addWidget(self.status_label)
        left_layout.addStretch()

        self.btn_park = QPushButton("画车位（标准）")
        self.btn_forward = QPushButton("整机自检")
        self.btn_backward = QPushButton("水泵清洗")
        self.btn_left = QPushButton("画车位（路沿）")
        self.btn_right = QPushButton("二次喷涂")
        self.btn_stop = QPushButton("紧急停止")

        self.control_buttons = [
            self.btn_park,
            self.btn_forward, self.btn_backward,
            self.btn_left, self.btn_right, self.btn_stop
        ]

        for btn in self.control_buttons:
            btn.clicked.connect(self.send_button)
            btn.setMinimumHeight(40)
            btn.setStyleSheet("font-size:20px;")
            left_layout.addWidget(btn)

        self.btn_settings = QPushButton("设置")
        self.btn_settings.setMinimumHeight(40)
        self.btn_settings.setStyleSheet("font-size:20px;")
        self.btn_settings.clicked.connect(self.show_settings)
        left_layout.addWidget(self.btn_settings)

        main_layout.addLayout(left_layout, 1)

        # ---------------- 中间（发送日志） ----------------
        center_layout = QVBoxLayout()
        self.send_log_label = QLabel("发送日志")
        self.send_log = QTextEdit()
        self.send_log.setReadOnly(True)
        center_layout.addWidget(self.send_log_label)
        center_layout.addWidget(self.send_log, stretch=1)

        self.btn_clear_send = QPushButton("清空发送日志")
        self.btn_clear_send.clicked.connect(self.clear_send_log)
        center_layout.addWidget(self.btn_clear_send)

        self.send_mode_box = QComboBox()
        self.send_mode_box.addItems(["自动", "HEX", "ASCII"])
        self.send_mode_box.setStyleSheet("""
            QComboBox { font-size:14px; min-height:30px; }
            QComboBox QAbstractItemView { font-size:14px; }
        """)
        center_layout.addWidget(QLabel("发送模式:"))
        center_layout.addWidget(self.send_mode_box)

        self.send_pause_checkbox = QCheckBox("暂停自动滚动")
        center_layout.addWidget(self.send_pause_checkbox)

        main_layout.addLayout(center_layout, 1)

        # ---------------- 右侧（接收日志） ----------------
        right_layout = QVBoxLayout()
        self.recv_log_label = QLabel("接收日志")
        self.recv_log = QTextEdit()
        self.recv_log.setReadOnly(True)
        right_layout.addWidget(self.recv_log_label)
        right_layout.addWidget(self.recv_log, stretch=1)

        self.btn_clear_recv = QPushButton("清空接收日志")
        self.btn_clear_recv.clicked.connect(self.clear_recv_log)
        right_layout.addWidget(self.btn_clear_recv)

        self.recv_mode_box = QComboBox()
        self.recv_mode_box.addItems(["自动", "HEX", "ASCII"])
        self.recv_mode_box.setStyleSheet("""
            QComboBox { font-size:14px; min-height:30px; }
            QComboBox QAbstractItemView { font-size:14px; }
        """)
        right_layout.addWidget(QLabel("接收模式:"))
        right_layout.addWidget(self.recv_mode_box)

        self.recv_pause_checkbox = QCheckBox("暂停自动滚动")
        right_layout.addWidget(self.recv_pause_checkbox)

        main_layout.addLayout(right_layout, 1)
        self.stack_layout.addWidget(self.main_widget)

        # ---------------- 设置界面 ----------------
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

        self.stack_layout.addWidget(self.settings_widget)

    # ---------------- 页面切换 ----------------
    def show_settings(self):
        self.setWindowTitle("设置")
        self.stack_layout.setCurrentWidget(self.settings_widget)

    def show_main(self):
        self.setWindowTitle("道路画线机控制面板")
        self.stack_layout.setCurrentWidget(self.main_widget)

    # ---------------- 发送/接收日志 ----------------
    def clear_send_log(self):
        self.send_log.clear()

    def clear_recv_log(self):
        self.recv_log.clear()

    def append_send_log(self, data, label=""):
        if not self.send_pause_checkbox.isChecked():
            try:
                text = f"{data.decode()} ({label})"
            except:
                text = f"{data.hex()} ({label})"
            self.append_send_signal.emit(text)

    def _append_send(self, text):
        self.send_log.append(text)
        self.send_log.moveCursor(QTextCursor.End)

    def on_data_received(self, data):
        results = self.protocol.parse(data)
        for item in results:
            self.append_recv_signal.emit(item)

    def _append_recv(self, item):
        if not self.recv_pause_checkbox.isChecked():
            mode = self.recv_mode_box.currentText()
            if mode == "HEX":
                text = f"{item['raw']} ({item['status']})"
            elif mode == "ASCII":
                text = f"{item['char']} ({item['status']})"
            else:
                if 32 <= item['value'] <= 126:
                    text = f"{item['char']} ({item['status']})"
                else:
                    text = f"{item['raw']} ({item['status']})"
            self.recv_log.append(text)
            self.recv_log.moveCursor(QTextCursor.End)
        self.status_label.setText(f"状态:\n{item['status']}")

    # ---------------- 核心按钮功能 ----------------
    def send_button(self):
        btn = self.sender()
        cmd_map = {
            "画车位（标准）": b'P',
            "整机自检": b'D',
            "水泵清洗": b'K',
            "画车位（路沿）": b'N',
            "二次喷涂": b'C',
            "紧急停止": b'T'
        }
        data = cmd_map.get(btn.text(), b'')
        if data:
            self.serial_mgr.send(data)
            self.append_send_log(data, btn.text())
            self.status_label.setText(f"状态:\n{btn.text()}")

            # ---------------- 摄像头弹窗 ----------------
            if btn.text() in ["二次喷涂", "画车位（路沿）"]:
                if hasattr(self, 'camera_window') and self.camera_window.isVisible():
                    self.camera_widget.stop_camera()
                    self.camera_window.close()
                self.open_camera_window()

            # ---------------- 紧急停止 ----------------
            if btn.text() == "紧急停止":
                if hasattr(self, 'camera_window') and self.camera_window.isVisible():
                    self.camera_widget.stop_camera()
                    self.camera_window.close()

    # ---------------- 摄像头弹窗 ----------------
    def open_camera_window(self):
        self.camera_window = QWidget()
        self.camera_window.setWindowTitle("摄像头画面")
        self.camera_window.resize(800, 600)
        layout = QVBoxLayout()
        self.camera_window.setLayout(layout)
        self.camera_widget = CameraWidget()
        layout.addWidget(self.camera_widget)
        self.camera_widget.start_camera()
        self.camera_window.closeEvent = self.camera_close_event
        self.camera_window.show()

    def camera_close_event(self, event):
        if hasattr(self, 'camera_widget'):
            self.camera_widget.stop_camera()
        event.accept()