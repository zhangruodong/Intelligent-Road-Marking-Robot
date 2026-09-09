class Protocol:
    """V650 11字节帧协议"""

    FRAME_HEADER = 0xFD
    FRAME_TAIL   = 0xDF

    CMD_VEL    = 0   # 运动控制
    CMD_SERVO  = 4   # 舵机角度

    def __init__(self):
        self.buffer = bytearray()
        self.frame_len = 11

    # ========== 构建发送帧 ==========

    def build_vel_frame(self, vx: float, vy: float, vz: float) -> bytes:
        """速度指令：Vx/Vy/Vz 单位 mm/s"""
        vx_mm = int(vx * 1000)
        vy_mm = int(vy * 1000)
        vz_mm = int(vz * 1000)

        frame = bytearray(self.frame_len)
        frame[0] = self.FRAME_HEADER
        frame[1] = self.CMD_VEL
        frame[2] = 0  # reserved
        frame[3] = (vx_mm >> 8) & 0xFF
        frame[4] = vx_mm & 0xFF
        frame[5] = (vy_mm >> 8) & 0xFF
        frame[6] = vy_mm & 0xFF
        frame[7] = (vz_mm >> 8) & 0xFF
        frame[8] = vz_mm & 0xFF
        frame[9] = self._bcc(frame)
        frame[10] = self.FRAME_TAIL
        return bytes(frame)

    def build_servo_frame(self, pulse_us: int) -> bytes:
        """舵机角度指令：pulse_us 500~2500"""
        pulse_us = max(500, min(2500, pulse_us))

        frame = bytearray(self.frame_len)
        frame[0] = self.FRAME_HEADER
        frame[1] = self.CMD_SERVO
        frame[2] = 0  # reserved
        frame[3] = (pulse_us >> 8) & 0xFF
        frame[4] = pulse_us & 0xFF
        for i in range(5, 9):
            frame[i] = 0  # unused
        frame[9] = self._bcc(frame)
        frame[10] = self.FRAME_TAIL
        return bytes(frame)

    def _bcc(self, frame: bytearray) -> int:
        """异或校验（第0~8字节）"""
        bcc = 0
        for i in range(9):
            bcc ^= frame[i]
        return bcc
