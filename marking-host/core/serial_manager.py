import serial
import threading
import time

from PyQt5.QtCore import QObject, pyqtSignal


class SerialManager(QObject):

    # 接收到数据
    data_received = pyqtSignal(bytes)

    # 错误信号
    error_signal = pyqtSignal(str)

    def __init__(self, port="/dev/ttyS0", baudrate=9600):
        super().__init__()

        self.port = port
        self.baudrate = baudrate

        self.ser = None
        self.running = False

    # ================= 打开串口 =================
    def open(self):

        try:
            self.ser = serial.Serial(
                self.port,
                self.baudrate,
                timeout=0.1
            )

            self.running = True

            threading.Thread(
                target=self._read_loop,
                daemon=True
            ).start()

            print(f"[串口] 已打开: {self.port} @ {self.baudrate}")

        except Exception as e:
            self.error_signal.emit(str(e))
            print("[串口] 打开失败:", e)

    # ================= 接收线程 =================
    def _read_loop(self):

        while self.running:

            try:

                if self.ser:

                    # 尽量保持帧完整
                    data = self.ser.read(1024)

                    if data:

                        # 发 Qt 信号
                        self.data_received.emit(data)

                time.sleep(0.005)

            except Exception as e:

                self.error_signal.emit(str(e))
                print("[串口] 读取异常:", e)

    # ================= 发送 =================
    def send(self, data: bytes):

        try:

            if self.ser:
                self.ser.write(data)
                print("[发送]", data)

        except Exception as e:

            self.error_signal.emit(str(e))
            print("[串口] 发送失败:", e)

    # ================= 关闭 =================
    def close(self):

        self.running = False

        if self.ser:

            self.ser.close()

            print("[串口] 已关闭")