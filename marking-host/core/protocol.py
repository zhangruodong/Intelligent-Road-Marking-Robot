# 下位机 HandleCmd 认识的全部指令（见 marking-firmware/Hardware/system.c）。
# 下位机收到什么就原样回显什么，所以这张表既是接收日志的翻译，
# 也是面板按钮取名字的依据 —— 改指令集时两边一起改。
COMMANDS = {
    ord('P'): "画车位（标准）",
    ord('N'): "画车位（路沿）",   # 下位机尚未实现，按下去是空操作（见 protocol_test.NOT_IMPLEMENTED）
    ord('C'): "二次喷涂",         # 相机闭环：检测到破损自动发，见 camera_widget.maybe_send_respray
    ord('D'): "整机自检",
    ord('K'): "水泵清洗",
    ord('G'): "关水泵",
    ord('W'): "停止复位",
    ord('T'): "紧急停止",
    ord('J'): "夹臂关",
    ord('F'): "蜂鸣提示",
    ord('U'): "前进",
    ord('R'): "后退",
    ord('Y'): "右转",
    ord('Z'): "左转",
    ord('A'): "左移",
    ord('X'): "右移",
    ord('+'): "加速",
    ord('-'): "减速",
}


# 面板按钮：(标签, 指令字节)。标签同时是 send_button 的查表键，所以按钮
# 只在这里定义一次 —— 原来标签在 init_ui、cmd_map、摄像头弹窗判断里各写了
# 一遍，改错一个字就有一个按钮悄悄失效（N/C 就是这么漏掉的）。
PANEL_BUTTONS = [
    ("画车位（标准）", b'P'),
    ("整机自检",       b'D'),
    ("水泵清洗",       b'K'),
    ("关水泵",         b'G'),
    ("画车位（路沿）", b'N'),
    ("二次喷涂",       b'C'),
    ("停止复位",       b'W'),
    ("紧急停止",       b'T'),
]

# 按下去要弹出摄像头画面的按钮
CAMERA_BUTTONS = {"二次喷涂", "画车位（路沿）"}

# 按下去要收起摄像头画面的按钮
CAMERA_OFF_BUTTONS = {"紧急停止"}


class Protocol:
    def __init__(self):
        self.buffer = bytearray()
        self.frame_len = 1  # STM32 每条消息一字节

    def parse(self, data: bytes):
        """
        每收到一个字节作为一条消息解析
        """
        results = []
        self.buffer.extend(data)

        while len(self.buffer) >= self.frame_len:
            frame = self.buffer[:self.frame_len]
            del self.buffer[:self.frame_len]

            value = frame[0]

            results.append({
                "raw": frame.hex(),
                "char": chr(value),
                "value": value,
                "status": COMMANDS.get(value, "未知")
            })
        return results
