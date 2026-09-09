import cv2
from PyQt5.QtWidgets import QLabel
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QImage, QPixmap
from core.tag_detector import TagDetector


class CameraWidget(QLabel):

    def __init__(self):
        super().__init__()

        self.setAlignment(Qt.AlignCenter)
        self.setText("摄像头未开启")

        self.cap = None
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)

        self.tag_detector = TagDetector()
        self.on_tag_seen = None   # 每帧回调

    # ================== 开关 ==================
    def start_camera(self):
        if self.cap is not None:
            return
        self.cap = cv2.VideoCapture(0)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

        if not self.cap.isOpened():
            self.setText("摄像头打开失败")
            return

        self.timer.start(30)

    def stop_camera(self):
        self.timer.stop()
        if self.cap:
            self.cap.release()
        self.cap = None
        self.clear()
        self.setText("摄像头已关闭")

    # ================== 帧处理 ==================
    def update_frame(self):
        if self.cap is None:
            return

        ret, frame = self.cap.read()
        if not ret:
            return

        # AprilTag 检测
        self.tag_detector.detect(frame)

        if self.tag_detector.found and self.on_tag_seen:
            self.on_tag_seen()

        # 绘制检测框
        frame = self.tag_detector.draw(frame)

        # 显示标签状态
        if self.tag_detector.found:
            cv2.putText(frame, f"TAG ID=0", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

        # 转为 QPixmap
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = frame.shape
        image = QImage(frame.data, w, h, ch * w, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(image)
        self.setPixmap(pixmap.scaled(
            self.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation))

    def closeEvent(self, event):
        self.stop_camera()
        event.accept()
