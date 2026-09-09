import cv2
import numpy as np
from collections import deque

cap = cv2.VideoCapture(0)

# 最近 N 帧中心线 x 坐标
center_queue = deque(maxlen=5)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w, _ = frame.shape

    # ---------------- ROI ----------------
    mask_roi = np.zeros_like(frame[:,:,0])
    polygon = np.array([[
        (0, h),
        (0, int(h*0.6)),
        (w, int(h*0.6)),
        (w, h)
    ]], np.int32)
    cv2.fillPoly(mask_roi, polygon, 255)
    roi = cv2.bitwise_and(frame, frame, mask=mask_roi)

    # ---------------- HSV 黄色 ----------------
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    lower_yellow = np.array([20, 150, 150])
    upper_yellow = np.array([35, 255, 255])
    mask_yellow = cv2.inRange(hsv, lower_yellow, upper_yellow)

    # ---------------- 模糊 + Canny ----------------
    blur = cv2.GaussianBlur(mask_yellow, (5,5), 0)
    edges = cv2.Canny(blur, 50, 150)

    # ---------------- 霍夫直线 ----------------
    lines = cv2.HoughLinesP(edges, 1, np.pi/180, threshold=50, minLineLength=50, maxLineGap=20)
    line_img = np.zeros_like(frame)
    center_x = []

    if lines is not None:
        for x1, y1, x2, y2 in lines[:,0]:
            angle = np.arctan2(y2-y1, x2-x1) * 180 / np.pi
            if 75 < abs(angle) < 105:
                cv2.line(line_img, (x1,y1), (x2,y2), (0,0,255), 3)
                center_x.append((x1 + x2) // 2)

    # ---------------- 平滑中心线 ----------------
    if center_x:
        avg_x = int(np.mean(center_x))
        center_queue.append(avg_x)
        smooth_x = int(np.mean(center_queue))
        cv2.line(line_img, (smooth_x, h), (smooth_x, int(h*0.6)), (0,255,0), 4)

    result = cv2.addWeighted(frame, 0.8, line_img, 1, 0)
    cv2.imshow("Yellow Center Line - Stable", result)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()