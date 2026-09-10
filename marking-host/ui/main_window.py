import cv2
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QTextEdit, QLabel,
    QComboBox, QCheckBox, QSlider, QStackedLayout, QSpacerItem, QSizePolicy
)
from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QTextCursor
from ui.camera_widget import CameraWidget
from core.protocol import PANEL_BUTTONS, CAMERA_BUTTONS, CAMERA_OFF_BUTTONS

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
        # error_signal 原来一个槽都没接：串口没打开时 send() 是静默失败，
        # 发送日志照记、摄像头上的 spray 计数照涨，看着像在干活，
        # 其实一个字节都没上总线。
        self.serial_mgr.error_signal.connect(self.on_serial_error)
        self.serial_mgr.open()

    def on_serial_error(self, msg):
        self.append_send_signal.emit(f"串口错误: {msg} —— 字节没发出去")

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

        # 按钮表在 core/protocol.py 里，和指令表放在一起，避免标签写两遍对不上
        self.control_buttons = []
        for label, _cmd in PANEL_BUTTONS:
            btn = QPushButton(label)
            btn.clicked.connect(self.send_button)
            btn.setMinimumHeight(40)
            btn.setStyleSheet("font-size:20px;")
            left_layout.addWidget(btn)
            self.control_buttons.append(btn)

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

        # 这里原来有个"发送模式"下拉框，但发送路径根本不读它（指令固定是
        # 单字节 ASCII），是个死控件，删掉。接收模式那个是真的在用，保留。
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
        label = self.sender().text()
        data = dict(PANEL_BUTTONS).get(label)
        if data is None:
            return

        self.serial_mgr.send(data)
        self.append_send_log(data, label)
        self.status_label.setText(f"状态:\n{label}")

        # 按了面板上别的键 = 不想让它自己喷了，先把相机闭环撤下来。
        # 不撤的话按"停止复位"或者"关水泵"，2 秒后相机又自己开喷 ——
        # 人按的停停不停得住全看手速，这不能接受。要接着喷就再按一次
        # "二次喷涂"（下面会重新开一个武装好的窗口）。
        # 下位机那边也堵了一道：'C' 只在 STATE_WAIT 被接受。
        widget = getattr(self, 'camera_widget', None)
        if widget is not None and label not in CAMERA_BUTTONS:
            widget.auto_paint = False

        # ---------------- 摄像头弹窗 ----------------
        if label in CAMERA_BUTTONS:
            self.close_camera_window()
            self.open_camera_window(label)
        elif label in CAMERA_OFF_BUTTONS:
            self.close_camera_window()

    # ---------------- 摄像头弹窗 ----------------
    def close_camera_window(self):
        win = getattr(self, 'camera_window', None)
        if win is None:
            return
        self.camera_widget.stop_camera()
        win.close()
        win.deleteLater()      # 不然每按一次 N/C 就漏一个 QWidget
        self.camera_window = None
        self.camera_widget = None

    def open_camera_window(self, label):
        self.camera_window = QWidget()
        self.camera_window.setWindowTitle("摄像头画面")
        self.camera_window.resize(800, 600)
        layout = QVBoxLayout()
        self.camera_window.setLayout(layout)
        self.camera_widget = CameraWidget(
            self.serial_mgr,
            # 只有"二次喷涂"是相机闭环：看到破损就自动发 C。
            # "画车位（路沿）"只是借这个窗口取景，一个字节都不发。
            auto_paint=(label == "二次喷涂"),
        )
        layout.addWidget(self.camera_widget)
        self.camera_widget.start_camera()
        self.camera_window.closeEvent = self.camera_close_event
        self.camera_window.show()

    # ---------------- 主窗口关闭 ----------------
    def closeEvent(self, event):
        # 摄像头窗口是没有父窗口的顶层窗口（camera_window = QWidget()），
        # 主界面关掉它不会跟着关，还会继续自己发 'C'。
        self.close_camera_window()
        event.accept()

    def camera_close_event(self, event):
        # 用户点右上角 X 时走这里。子控件的 closeEvent 不一定会触发
        # （父窗口关闭只是把子控件隐藏），所以摄像头必须在这里自己停。
        widget = getattr(self, 'camera_widget', None)
        if widget is not None:
            widget.stop_camera()
        event.accept()