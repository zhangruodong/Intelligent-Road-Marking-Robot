"""
AprilTag 检测器 — 识别充电桩标签
使用 OpenCV ArUco 模块
"""

import cv2
import numpy as np

# 标签类型: 36h11 系列, 用 ID=0 作为充电桩标记
TAG_FAMILY = cv2.aruco.DICT_36H11
TAG_ID = 0     # 充电桩用哪个 ID


class TagDetector:
    """封装 AprilTag 检测, 给 dock_controller 和 camera_widget 用"""

    def __init__(self):
        self.dict = cv2.aruco.getPredefinedDictionary(TAG_FAMILY)
        self.params = cv2.aruco.DetectorParameters()
        self.detector = cv2.aruco.ArucoDetector(self.dict, self.params)

        self.found = False        # 当前帧是否检测到
        self.corners = None       # 四角坐标
        self.center_x = -1        # 标签中心 X
        self.frame_w = 640

    def detect(self, frame: np.ndarray):
        """对一帧做 AprilTag 检测, 返回是否找到"""
        self.found = False
        self.corners = None

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        corners, ids, _ = self.detector.detectMarkers(gray)

        if ids is not None:
            for i, tag_id in enumerate(ids):
                if tag_id[0] == TAG_ID:
                    self.found = True
                    self.corners = corners[i][0]
                    # 中心坐标
                    cx = np.mean(self.corners[:, 0])
                    cy = np.mean(self.corners[:, 1])
                    self.center_x = int(cx)
                    self.frame_w = frame.shape[1]
                    return True
        return False

    def get_offset_from_center(self) -> float:
        """
        返回标签中心偏离画面中心的归一化值 [-1, 1]
        0 = 正中, -1 = 完全偏左, 1 = 完全偏右
        """
        if not self.found or self.frame_w == 0:
            return 0.0
        return (self.center_x - self.frame_w / 2) / (self.frame_w / 2)

    def draw(self, frame: np.ndarray) -> np.ndarray:
        """在帧上绘制检测结果"""
        if self.found and self.corners is not None:
            cv2.aruco.drawDetectedMarkers(frame, np.array([[self.corners]]), np.array([[TAG_ID]]))
            cv2.circle(frame, (self.center_x, frame.shape[0] // 2), 5, (0, 255, 0), -1)
        return frame
