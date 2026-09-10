"""
标线破损判据自检。用合成 mask，没有素材也能跑。

每个用例都对着真实路面照（4960.jpg_wh860.jpg）上量到的一个数：
裂缝宽度、双黄线间隔、碎块长宽比。逻辑一坏就在这里响。

    python3 vision_test.py     # 或 pytest vision_test.py
"""

import cv2
import numpy as np

from core.line_quality import (
    THRESHOLDS, close_cracks, evaluate_line_quality,
)


def blank(h=400, w=80):
    return np.zeros((h, w), np.uint8)


def healthy(h=400, w=80, x0=30, x1=50):
    m = blank(h, w)
    m[:, x0:x1] = 255
    return m


def cov_of(reason):
    """从说明串里把 cov= 抠出来，用来断言覆盖率本身而不只是结论。"""
    return float(reason.split("cov=")[1].split()[0])


def best_aspect(mask):
    """最大轮廓的长宽比，对应 camera_widget.is_line_shape 的判据。"""
    cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    best = 0.0
    for c in cnts:
        if cv2.contourArea(c) < 200:
            continue
        (_, (rw, rh), _) = cv2.minAreaRect(c)
        if rw and rh:
            best = max(best, max(rw, rh) / min(rw, rh))
    return best


def test_close_cracks_makes_fragmented_line_selectable():
    """根因 1：裂缝把黄漆啃碎之后，不闭运算一个轮廓都选不中。

    实测真实照片上最大轮廓长宽比塌到 1.34，is_line_shape 要 4.0。
    烂得越狠越选不出来，正好反了 —— 这条测试就是钉住这个。
    """
    m = healthy()

    for y in range(10, 400, 25):     # 横向裂缝，每 25px 一道
        m[y:y + 6, :] = 0

    m[:, 39:41] = 0                  # 纵向裂缝，顺带把线宽劈开

    assert best_aspect(m) < 4.0, "前提不成立：碎成这样本来就应该选不中"
    assert best_aspect(close_cracks(m)) >= 4.0, "闭运算之后应该能选成标线"


def test_healthy_line_is_ok():
    """完好的线不能报要喷 —— 否则车一路狂喷。"""
    need, reason = evaluate_line_quality(healthy())
    assert not need, f"完好的线被误报: {reason}"
    assert reason.startswith("OK"), reason


def test_broken_line_is_flagged():
    """断掉一整段 = Break。"""
    m = healthy()
    m[150:250, :] = 0
    need, reason = evaluate_line_quality(m)
    assert need, f"断线没报: {reason}"
    assert reason.startswith("Break"), reason


def test_worn_line_is_flagged():
    """漆掉了一半 = 要喷（走 Break 还是 Worn 都行，都是要喷）。"""
    m = healthy()
    for y in range(0, 400, 40):
        m[y:y + 20, :] = 0
    need, reason = evaluate_line_quality(m)
    assert need, f"掉漆没报: {reason}"


def test_double_line_not_falsely_worn():
    """根因 2 的系统性偏差：两条黄线中间的空隙不能被算成"没漆"。

    实测双黄线间隔 12~25px，不按股分开的话这些空隙会把覆盖率压下去，
    健康的双黄线会永远误报该补。
    """
    m = blank()
    m[:, 10:35] = 255      # 第一条
    m[:, 55:80] = 255      # 第二条，中间空 20px

    need, reason = evaluate_line_quality(m)
    assert not need, f"健康的双黄线被误报: {reason}"
    assert cov_of(reason) > 0.9, f"中间空隙被算进没漆了: {reason}"


if __name__ == "__main__":
    for name in sorted(n for n in globals() if n.startswith("test_")):
        globals()[name]()
        print("ok", name)
    print("全部通过")
