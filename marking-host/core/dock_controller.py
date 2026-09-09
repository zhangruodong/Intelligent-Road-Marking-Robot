"""
自动回充状态机 — 舵机扫描 + 车身旋转 + 退桩对接
"""

from PyQt5.QtCore import QTimer


class DockState:
    IDLE       = 0  # 待机
    SWEEPING   = 1  # 舵机扫描中
    ROTATING   = 2  # 车身旋转(车尾对准桩)
    BACKING    = 3  # 倒退对接
    DONE       = 4  # 完成


class DockController:
    """非阻塞状态机, QTimer 30ms 心跳"""

    SERVO_MID   = 1500  # 舵机中点(摄像头朝前)
    SERVO_RIGHT = 2400  # 右极限 (约 +162°)
    SERVO_LEFT  = 600   # 左极限 (约 -162°)
    STEP        = 8     # 每步 8us

    def __init__(self, protocol):
        self.proto = protocol
        self.state = DockState.IDLE
        self.servo_pos = self.SERVO_MID
        self.servo_target = self.SERVO_MID
        self.sweep_dir = 1      # 1=右扫, -1=左扫
        self.tag_seen = False
        self.tag_servo_pos = 0  # 识别到标签时的舵机位置

        # 对外回调
        self.on_servo_cmd = None   # fn(pulse_us) → 发串口
        self.on_vel_cmd = None     # fn(vx,vy,vz)  → 发串口
        self.on_tag_found = None   # fn()           → 通知界面
        self.on_done = None        # fn()           → 对接完成

        self.timer = QTimer()
        self.timer.timeout.connect(self._tick)
        self.timer.start(30)

    # ==================== 开始扫描 ====================
    def start(self):
        self.state = DockState.SWEEPING
        self.servo_target = self.SERVO_RIGHT
        self.sweep_dir = 1
        self._step_servo()

    def stop(self):
        self.state = DockState.IDLE

    # ==================== 从外部通知检测到 Tag ====================
    def tag_detected(self):
        """camera_widget 每帧调用"""
        if self.state == DockState.SWEEPING:
            self.tag_seen = True
            self.tag_servo_pos = self.servo_pos

    # ==================== 心跳 ====================
    def _tick(self):
        if self.state == DockState.IDLE:
            return

        # —— 舵机走动一步 ——
        if self._step_servo():
            # 舵机这步走完了
            if self.state == DockState.SWEEPING and self.tag_seen:
                self._on_tag_during_sweep()
            elif self.state == DockState.ROTATING:
                self._on_tracking()

    # ==================== 舵机走一步 ====================
    def _step_servo(self):
        if self.servo_pos == self.servo_target:
            return False  # 已经到位

        diff = self.servo_target - self.servo_pos
        step = self.STEP if diff > 0 else -self.STEP
        if abs(diff) < self.STEP:
            step = diff

        self.servo_pos += step
        if self.on_servo_cmd:
            self.on_servo_cmd(self.servo_pos)
        return True

    # ==================== 扫描到尽头 ====================
    def _step_servo_done(self):
        """舵机走到目标位置了"""
        if self.state == DockState.SWEEPING:
            if self.sweep_dir == 1:
                # 扫完右边, 换左边
                self.servo_target = self.SERVO_LEFT
                self.sweep_dir = -1
            elif self.sweep_dir == -1:
                # 左右都扫完了, 没找到
                # 车身转 90° 再扫一遍(覆盖盲区)
                if not self.tag_seen:
                    if self.on_vel_cmd:
                        self.on_vel_cmd(0, 0, 0.2)  # 原地转
                    # 等车转 90° 后重新扫
                    self.servo_target = self.SERVO_RIGHT
                    self.sweep_dir = 1

    # ==================== 识别到 Tag ====================
    def _on_tag_during_sweep(self):
        """扫描中识别到标签"""
        self.state = DockState.ROTATING

        # 计算车体需要旋转的角度
        # tag_servo_pos: 舵机脉宽, 1500=正前方
        # 偏移角 = (tag_servo_pos - 1500) / 1500us * 270°/2
        offset_deg = (self.tag_servo_pos - self.SERVO_MID) / 1500.0 * 135.0

        # 车尾对准标签：需要转 180° - offset_deg
        self._car_rotate_deg = 180.0 - offset_deg

        # 开始旋转车身, 舵机目标回正到中点(摄像头锁定标签)
        self.servo_target = self.SERVO_MID

        if self.on_vel_cmd:
            dir_sign = 1 if self._car_rotate_deg > 0 else -1
            self.on_vel_cmd(0, 0, dir_sign * 0.15)  # 慢转

        if self.on_tag_found:
            self.on_tag_found()

    def _on_tracking(self):
        """旋转跟踪: 舵机回正中, 车体在转"""
        if abs(self.servo_pos - self.SERVO_MID) < self.STEP * 2:
            # 舵机已回正 → 停止旋转, 开始倒退
            if self.on_vel_cmd:
                self.on_vel_cmd(0, 0, 0)  # 停止旋转
            self.state = DockState.BACKING
            if self.on_vel_cmd:
                self.on_vel_cmd(-0.1, 0, 0)  # 倒退靠近
            self.state = DockState.DONE
            if self.on_done:
                self.on_done()
