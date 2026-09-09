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
            # 状态解析示例
            status = {
                ord('D'): "整机自检",
                ord('K'): "水泵清洗",
                ord('N'): "画车位（路沿）",
                ord('C'): "二次喷涂",
                ord('T'): "紧急停止",
                ord('P'): "画车位（标准）" 
            }.get(value, "未知")

            results.append({
                "raw": frame.hex(),
                "char": chr(value),
                "value": value,
                "status": status
            })
        return results
