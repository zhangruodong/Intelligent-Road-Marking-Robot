"""
camera_widget 自检。跑它才知道"检测到破损 -> 真的发出去了一个 C"这条路是通的：

    python3 camera_widget_test.py     # 或 pytest camera_widget_test.py

这条链是断过的：控件压根没拿 serial_mgr，need_paint 只画在画面上，一个字节都没上总线。
下面三条断言分别盯着链路、限流和撤下开关。

Qt 不是必须的：没有 PyQt5 就就地补一个只够本文件用的替身（见 _install_stub），
所以这台没装 PyQt5 的开发机也能跑。
"""

import os
import sys
import types
from pathlib import Path

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

HAVE_REAL_QT = True
try:
    import PyQt5  # noqa: F401
except ImportError:
    HAVE_REAL_QT = False


def _install_stub():
    """补上 camera_widget 用到的那几个 Qt 名字，别的什么都不做。

    这不是"模拟 Qt"，只是让模块能 import、setPixmap 之类不炸 ——
    被验证的是 update_frame 里的检测与闭环逻辑，那些和 Qt 无关。
    """
    class _Sig:
        def connect(self, *a):
            pass

    class QLabel:
        def __init__(self, *a, **k): pass
        def setAlignment(self, *a): pass
        def setText(self, *a): pass
        def setPixmap(self, *a): pass
        def clear(self): pass
        def size(self): return (640, 480)

    class QTimer:
        def __init__(self, *a, **k): self.timeout = _Sig()
        def start(self, *a): pass
        def stop(self, *a): pass

    class _Qt:
        AlignCenter = KeepAspectRatio = SmoothTransformation = 0

    class QImage:
        Format_RGB888 = 0
        def __init__(self, *a, **k): pass

    class QPixmap:
        @staticmethod
        def fromImage(img): return QPixmap()
        def scaled(self, *a): return self

    for mod, attrs in (
        ("PyQt5", {}),
        ("PyQt5.QtWidgets", {"QLabel": QLabel}),
        ("PyQt5.QtCore", {"QTimer": QTimer, "Qt": _Qt}),
        ("PyQt5.QtGui", {"QImage": QImage, "QPixmap": QPixmap}),
    ):
        m = types.ModuleType(mod)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[mod] = m


if not HAVE_REAL_QT:
    _install_stub()

import ui.camera_widget as camera_widget  # noqa: E402  必须在替身之后


_app = None
if HAVE_REAL_QT:
    # 真的 Qt 下 QLabel 之前必须先有 QApplication，否则 "Must construct a
    # QApplication before a QWidget" 直接 abort。树莓派上可能没有 DISPLAY。
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PyQt5.QtWidgets import QApplication
    _app = QApplication.instance() or QApplication([])


class FakeSerial:
    def __init__(self):
        self.sent = []

    def send(self, data):
        self.sent.append(data)


def line_frame(length=380, y0=50):
    """一帧带竖黄线的画面，长宽比够格被认成线状。"""
    img = np.zeros((480, 640, 3), np.uint8)
    cv2.line(img, (320, y0), (320, y0 + length), (0, 255, 255), 25)
    return img


def blank_frame():
    return np.zeros((480, 640, 3), np.uint8)


def feed(widget, frame):
    """把一帧塞进 update_frame —— cap 换成一个只会回这一帧的假摄像头。"""
    widget.cap = type("Cap", (), {"read": lambda self: (True, frame)})()
    widget.update_frame()


def test_queue_clears_when_line_is_lost():
    """认不到线必须清空平滑队列。

    不清的话，线跳走再回来、或者换到旁边那条，队列里最多 5 帧的旧中心会被和
    新中心一起平均 —— ROI 落在上一处，判出来的破损也是那一处的。
    """
    w = camera_widget.CameraWidget()

    for _ in range(3):
        feed(w, line_frame())
    assert len(w.center_queue) == 3, "认到线应当进队列"

    feed(w, blank_frame())
    assert len(w.center_queue) == 0, "没认到线时没清空队列 —— 会拿着旧中心算 ROI"

    feed(w, line_frame())
    assert len(w.center_queue) == 1


def test_broken_line_sends_one_byte_per_interval():
    """判破损就发 'C'，且按 SPRAY_INTERVAL_S 限流。"""
    s = FakeSerial()
    w = camera_widget.CameraWidget(serial_mgr=s, auto_paint=True)
    w.SPRAY_INTERVAL_S = 1000.0     # 上限流不用真的等一个周期

    # 强制判破损：这条测的是"判出来之后发不发得出去"，判据本身在 vision_test.py
    orig = camera_widget.evaluate_line_quality
    camera_widget.evaluate_line_quality = lambda px: (True, "worn")
    try:
        for _ in range(3):
            feed(w, line_frame())
        assert s.sent == [b'C'], f"限流没生效或有帧没发: {s.sent}"

        w.last_spray = 0.0          # 假装周期过了
        feed(w, line_frame())
        assert s.sent == [b'C', b'C'], s.sent
    finally:
        camera_widget.evaluate_line_quality = orig


def test_auto_paint_off_sends_nothing():
    """只是取景（画车位（路沿）那个窗口）时一个字节都不该发。"""
    s = FakeSerial()
    w = camera_widget.CameraWidget(serial_mgr=s, auto_paint=False)

    orig = camera_widget.evaluate_line_quality
    camera_widget.evaluate_line_quality = lambda px: (True, "worn")
    try:
        for _ in range(3):
            feed(w, line_frame())
    finally:
        camera_widget.evaluate_line_quality = orig

    assert s.sent == [], s.sent


def test_no_serial_manager_does_not_crash():
    """没接串口时（比如单独调试）不能炸，只是不发。"""
    w = camera_widget.CameraWidget(serial_mgr=None, auto_paint=True)
    feed(w, line_frame())
    feed(w, blank_frame())


if __name__ == "__main__":
    for name in sorted(n for n in globals() if n.startswith("test_")):
        globals()[name]()
        print("ok", name)
    print("全部通过")
