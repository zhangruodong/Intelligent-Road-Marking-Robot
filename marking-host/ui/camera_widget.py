import cv2
import numpy as np
from collections import deque

from PyQt5.QtWidgets import QLabel
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QImage, QPixmap


class CameraWidget(QLabel):

    def __init__(self):
        super().__init__()

        self.setAlignment(Qt.AlignCenter)
        self.setText("摄像头未开启")

        self.cap = None

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)

        # ================= 平滑队列 =================
        self.center_queue = deque(maxlen=5)

        # ================= HSV 黄色范围 =================
        self.HSV_YELLOW_LOW = np.array([15, 50, 150])
        self.HSV_YELLOW_HIGH = np.array([30, 255, 255])

        # ================= 参数 =================
        self.RECT_WIDTH = 100
        self.MIN_AREA = 200
        self.LINEAR_ASPECT_RATIO_THRESH = 4.0

        self.CRACK_RATIO_THRESHOLD = 0.02
        self.IMPURITY_RATIO_THRESHOLD = 0.03
        self.SOLIDITY_THRESHOLD = 0.7

    # ==========================================================
    # 摄像头开启
    # ==========================================================
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

    # ==========================================================
    # 摄像头关闭
    # ==========================================================
    def stop_camera(self):

        self.timer.stop()

        if self.cap:
            self.cap.release()

        self.cap = None

        self.clear()

        self.setText("摄像头已关闭")

    # ==========================================================
    # PCA 主方向
    # ==========================================================
    def get_orientation_pca(self, points):

        mean = np.mean(points, axis=0)

        centered = points - mean

        cov = np.cov(centered.T)

        eig_vals, eig_vecs = np.linalg.eig(cov)

        main_dir = eig_vecs[:, np.argmax(eig_vals)]

        angle = np.arctan2(
            main_dir[1],
            main_dir[0]
        ) * 180.0 / np.pi

        if angle > 90:
            angle -= 180

        elif angle < -90:
            angle += 180

        return mean, angle

    # ==========================================================
    # 判断是否为线状
    # ==========================================================
    def is_line_shape(self, contour):

        rect = cv2.minAreaRect(contour)

        (w, h) = rect[1]

        if w == 0 or h == 0:
            return False

        aspect_ratio = max(w, h) / min(w, h)

        return aspect_ratio >= self.LINEAR_ASPECT_RATIO_THRESH

    # ==========================================================
    # 评估标线质量
    # ==========================================================
    def evaluate_centerline_quality(self, line_mask):

        total_pixels = cv2.countNonZero(line_mask)

        if total_pixels < 50:
            return True, "No line"

        contours, _ = cv2.findContours(
            line_mask,
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE
        )

        if not contours:
            return True, "No contour"

        main_contour = max(contours, key=cv2.contourArea)

        area = cv2.contourArea(main_contour)

        if area < 50:
            return True, "Too small"

        # ================= 实心度 =================

        hull = cv2.convexHull(main_contour)

        hull_area = cv2.contourArea(hull)

        solidity = area / hull_area if hull_area > 0 else 0

        if solidity < self.SOLIDITY_THRESHOLD:
            return True, f"Low solidity"

        # ================= 裂缝检测 =================

        kernel_close = np.ones((3, 3), np.uint8)

        closed = cv2.morphologyEx(
            line_mask,
            cv2.MORPH_CLOSE,
            kernel_close
        )

        crack_diff = cv2.absdiff(line_mask, closed)

        crack_area = cv2.countNonZero(crack_diff)

        crack_ratio = crack_area / area

        if crack_ratio > self.CRACK_RATIO_THRESHOLD:
            return True, f"Crack"

        # ================= 杂质检测 =================

        kernel_open = np.ones((3, 3), np.uint8)

        opened = cv2.morphologyEx(
            line_mask,
            cv2.MORPH_OPEN,
            kernel_open
        )

        impurity_diff = cv2.absdiff(line_mask, opened)

        impurity_area = cv2.countNonZero(impurity_diff)

        impurity_ratio = impurity_area / area

        if impurity_ratio > self.IMPURITY_RATIO_THRESHOLD:
            return True, f"Impurity"

        return False, "Good"

    # ==========================================================
    # 绘制定向矩形
    # ==========================================================
    def draw_oriented_rect(
            self,
            image,
            center,
            angle,
            length,
            width,
            color,
            thickness=2
    ):

        rect = np.array([
            [-length / 2, -width / 2],
            [length / 2, -width / 2],
            [length / 2, width / 2],
            [-length / 2, width / 2]
        ])

        rad = np.deg2rad(angle)

        rot = np.array([
            [np.cos(rad), -np.sin(rad)],
            [np.sin(rad), np.cos(rad)]
        ])

        rect_rot = np.dot(rect, rot.T) + center

        rect_rot = rect_rot.astype(np.int32)

        cv2.polylines(
            image,
            [rect_rot],
            True,
            color,
            thickness
        )

    # ==========================================================
    # 更新画面
    # ==========================================================
    def update_frame(self):

        if self.cap is None:
            return

        ret, frame = self.cap.read()

        if not ret:
            return

        result = frame.copy()

        # ======================================================
        # HSV 黄色提取
        # ======================================================

        hsv = cv2.cvtColor(
            frame,
            cv2.COLOR_BGR2HSV
        )

        yellow_mask = cv2.inRange(
            hsv,
            self.HSV_YELLOW_LOW,
            self.HSV_YELLOW_HIGH
        )

        # ======================================================
        # 开运算去噪
        # ======================================================

        kernel_open = np.ones((3, 3), np.uint8)

        yellow_mask = cv2.morphologyEx(
            yellow_mask,
            cv2.MORPH_OPEN,
            kernel_open
        )

        # ======================================================
        # 轮廓检测
        # ======================================================

        contours, _ = cv2.findContours(
            yellow_mask,
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE
        )

        if contours:

            contours_sorted = sorted(
                contours,
                key=cv2.contourArea,
                reverse=True
            )

            line_contour = None

            for cnt in contours_sorted:

                area = cv2.contourArea(cnt)

                if area < self.MIN_AREA:
                    continue

                if self.is_line_shape(cnt):
                    line_contour = cnt
                    break

            if line_contour is not None:

                line_mask = np.zeros_like(yellow_mask)

                cv2.drawContours(
                    line_mask,
                    [line_contour],
                    -1,
                    255,
                    -1
                )

                area = cv2.contourArea(line_contour)

                points = line_contour.reshape(-1, 2)

                center, angle = self.get_orientation_pca(points)

                cx, cy = int(center[0]), int(center[1])

                # ================= 平滑 =================

                self.center_queue.append((cx, cy))

                smooth_x = int(np.mean(
                    [p[0] for p in self.center_queue]
                ))

                smooth_y = int(np.mean(
                    [p[1] for p in self.center_queue]
                ))

                # ================= 喷涂判断 =================

                need_paint, reason = self.evaluate_centerline_quality(
                    line_mask
                )

                rect_color = (
                    (0, 0, 255)
                    if need_paint
                    else
                    (0, 255, 0)
                )

                label = (
                    "NEED PAINT"
                    if need_paint
                    else
                    "NO PAINT"
                )

                # ================= 计算矩形 =================

                rad = np.deg2rad(angle)

                dir_vec = np.array([
                    np.cos(rad),
                    np.sin(rad)
                ])

                projections = np.dot(points, dir_vec)

                proj_min = np.min(projections)

                proj_max = np.max(projections)

                line_length = proj_max - proj_min

                rect_length = max(line_length + 20, 80)

                # ================= 绘制矩形 =================

                self.draw_oriented_rect(
                    result,
                    (smooth_x, smooth_y),
                    angle,
                    rect_length,
                    self.RECT_WIDTH,
                    rect_color,
                    thickness=3
                )

                # ================= 显示文字 =================

                cv2.putText(
                    result,
                    label,
                    (smooth_x - 50, smooth_y - 20),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.8,
                    rect_color,
                    2
                )

                cv2.putText(
                    result,
                    reason,
                    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.7,
                    (255, 255, 255),
                    2
                )

        # ======================================================
        # Qt 显示
        # ======================================================

        result = cv2.cvtColor(
            result,
            cv2.COLOR_BGR2RGB
        )

        h, w, ch = result.shape

        bytes_per_line = ch * w

        image = QImage(
            result.data,
            w,
            h,
            bytes_per_line,
            QImage.Format_RGB888
        )

        pixmap = QPixmap.fromImage(image)

        self.setPixmap(
            pixmap.scaled(
                self.size(),
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation
            )
        )

    # ==========================================================
    # 关闭事件
    # ==========================================================
    def closeEvent(self, event):

        self.stop_camera()

        event.accept()