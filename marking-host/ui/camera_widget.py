import time

import cv2
import numpy as np
from collections import deque

from PyQt5.QtWidgets import QLabel
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QImage, QPixmap

from core.line_quality import (
    close_cracks, extract_line_pixels, evaluate_line_quality,
)


class CameraWidget(QLabel):

    def __init__(self, serial_mgr=None, auto_paint=False):
        super().__init__()

        self.setAlignment(Qt.AlignCenter)
        self.setText("摄像头未开启")

        self.cap = None

        # 检测到破损就往下位机发 'C' 补一段的闭环。默认关着：这个控件
        # 也被"画车位（路沿）"拿去取景，那种场合一个字节都不该发。
        self.serial_mgr = serial_mgr
        self.auto_paint = auto_paint

        # 下位机收到一条 'C' 会泵开直行一段（RESPRAY_PULSE 5000 ≈ 1.0m，
        # 默认速度下约 1.5s）跑完自己停。这个周期略大于一段的时间，
        # 喷出来才连得上；短了也安全（下位机只在 WAIT 接 'C'，一段跑着
        # 的时候发过去是被丢掉的），只是白费字节。改下位机那个常量这里一起改。
        self.SPRAY_INTERVAL_S = 2.0
        self.last_spray = 0.0
        self.spray_count = 0

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
        # 破损判据的阈值统一在 core/line_quality.py 的 THRESHOLDS 里，
        # 现场标定只改那一处，别在这儿再放一份。

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
            # 失败必须把句柄扔掉。留着一个没打开的 VideoCapture，开头那句
            # `if self.cap is not None: return` 会让之后每次 start_camera()
            # 都直接返回 —— 摄像头再也起不来（插上摄像头也没用），还漏个句柄。
            self.cap.release()
            self.cap = None
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

    # 破损判据在 core/line_quality.evaluate_line_quality()。
    # 移出去是因为那边不依赖 Qt，能单独跑 vision_test.py；
    # 而且要求喂【原始黄像素】—— 原来这里喂填实的剪影，
    # 量的其实是剪影的形状，跟漆面完好程度没关系。

    # ==========================================================
    # 破了 -> 发 'C' 让下位机补一段
    # ==========================================================
    def maybe_send_respray(self):
        """破损还在就每 SPRAY_INTERVAL_S 补发一条 'C'。

        一段是有界的（泵开直行固定脉冲后自动停），所以链路断了最多喷完
        当前这一段，不会失控 —— 上位机看不见下位机状态，只能靠这个。
        """
        if not self.auto_paint or self.serial_mgr is None:
            return

        now = time.monotonic()

        if now - self.last_spray < self.SPRAY_INTERVAL_S:
            return

        self.last_spray = now
        self.spray_count += 1
        self.serial_mgr.send(b'C')

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

        # 找轮廓要用闭运算补过裂缝的那份：破损处的黄漆被裂缝切成碎块，
        # 不补的话长宽比会塌到 1.x，is_line_shape 一个都选不中 ——
        # 于是烂得越狠越选不出来，正好反了。
        # 量覆盖率仍然用上面那份原始 yellow_mask，闭过的把掉漆也填平了。
        contours, _ = cv2.findContours(
            close_cracks(yellow_mask),
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE
        )

        found_line = False

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

                found_line = True

                points = line_contour.reshape(-1, 2)

                center, angle = self.get_orientation_pca(points)

                # ================= 计算矩形 =================

                rad = np.deg2rad(angle)

                dir_vec = np.array([
                    np.cos(rad),
                    np.sin(rad)
                ])

                projections = np.dot(points, dir_vec)

                line_length = np.max(projections) - np.min(projections)

                rect_length = max(line_length + 20, 80)

                # ================= 平滑 =================

                cx, cy = int(center[0]), int(center[1])

                self.center_queue.append((cx, cy))

                smooth_x = int(np.mean(
                    [p[0] for p in self.center_queue]
                ))

                smooth_y = int(np.mean(
                    [p[1] for p in self.center_queue]
                ))

                # ================= 喷涂判断 =================

                # 抠出矩形范围内的【原始黄像素】再判。不能拿闭过的那份，
                # 也不能拿填实的剪影 —— 两者都把掉漆填平了，判出来永远是"完好"。
                line_pixels = extract_line_pixels(
                    yellow_mask,
                    (smooth_x, smooth_y),
                    angle,
                    rect_length,
                    self.RECT_WIDTH
                )

                need_paint, reason = evaluate_line_quality(line_pixels)

                # ================= 闭环 =================

                if need_paint:
                    self.maybe_send_respray()

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

        if not found_line:
            # 这一帧没认出线（压根没轮廓，或者轮廓都不像线状）就把平滑队列清掉。
            # 不清的话，线跳走再回来、或者换到旁边那条线，队列里最多 5 帧的旧中心
            # 会被和新中心一起平均，ROI 落在上一处 —— 判出来的破损也是那一处的。
            # 方向是安全的（量不到就不喷），但喷的位置是错的。
            self.center_queue.clear()

        # ======================================================
        # 喷涂计数（闭环到底发出去没有，看这个数）
        # ======================================================

        # 一直画：撤下来（按了面板上别的键）得看得见，不然人以为还在自动喷。
        # 注意这个数只说明"我发了"，不说明下位机动了 —— 串口没打开时它也涨，
        # 那种情况看下位机的发送失败提示。
        cv2.putText(
            result,
            f"spray x{self.spray_count}" if self.auto_paint else "spray OFF",
            (10, 60),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 255, 255) if self.auto_paint else (128, 128, 128),
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