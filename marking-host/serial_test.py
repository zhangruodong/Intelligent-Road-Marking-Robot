import serial
import time

# 打开串口（/dev/serial0 = 排针串口，和 main.py 同一个口，见那边的注释）
ser = serial.Serial("/dev/serial0", 9600, timeout=1)
print("串口打开:", ser.is_open)

try:
    while True:
        # 读一个字节
        data = ser.read(1)
        if data:
            print("接收到:", data)
        time.sleep(0.01)
except KeyboardInterrupt:
    ser.close()
    print("串口关闭")
