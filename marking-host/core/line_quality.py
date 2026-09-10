"""
标线破损判据。纯 cv2/numpy，不依赖 Qt —— 所以 vision_test.py 在哪台机器上都能跑。

判据两条，都算在"标线自己的坐标系"里，所以远近一个标准：
  coverage  该有漆的地方实际有多少漆 —— 掉漆、褪色、缺块
  gap       有没有一整段断掉 —— 断裂

标定方法：在真实路面上跑，画面左上角会打出 "cov=.. gap=.. w=.." 三个数。
在"完好的线"和"该补的线"上各记几组，再回来改 THRESHOLDS。
下面的值是拿 4960.jpg_wh860.jpg 那张俯视实拍图定的初值，没在真车视角下标过。
"""

import cv2
import numpy as np


# ---------------- 标定参数（全工程唯一一份）----------------
THRESHOLDS = {
    # 闭运算核，用来补裂缝。裂缝竖向断口长度实测 中位数 3px、90分位 15px，
    # 所以高度取 15。宽度不能超过两条平行黄线之间的间隔（实测 12~25px），
    # 否则两条会被粘成一条，所以取 3。
    "CLOSE_KERNEL": (3, 15),

    # 分析窗口最长多少像素。别让它无限长：窗口越长，一段破损占的比例越小，
    # 断裂判据 MAX_GAP_RATIO 就越难触发。
    "MAX_ANALYSIS_LEN": 300.0,

    "SLAB_COVERAGE_MIN": 0.35,  # 单行像素数低于"典型满行"的这个比例 = 这行没漆
    "MIN_COVERAGE": 0.55,       # 沿线平均覆盖率低于此 = 掉漆
    "MAX_GAP_RATIO": 0.10,      # 最长连续没漆段占全长比例超过此 = 断裂

    "MIN_LINE_PIXELS": 150,     # 像素数低于此不当它是标线
    "MIN_LINE_LENGTH": 30.0,    # 沿线长度低于此不当它是标线
    "MIN_ROW_PIXELS": 3.0,      # 典型满行的像素数低于此 = 太细，不当它是标线
}


def close_cracks(yellow_mask):
    """闭运算补裂缝。**找轮廓必须用这份。**

    破损处的黄漆被裂缝啃成几百块小方片，最大轮廓的长宽比会塌到 1.x，
    is_line_shape 要求的 4.0 一个都过不了 —— 于是烂得越狠越选不出来，
    正好反了。补上闭运算，3x3 就能把长宽比从 1.34 拉回 7.35。

    注意：这只用来【找轮廓】。量覆盖率仍然要用没闭过的原始黄像素，
    闭运算会把掉漆一并填平，正则判据就废了。
    """
    kernel = cv2.getStructuringElement(
        cv2.MORPH_RECT, THRESHOLDS["CLOSE_KERNEL"]
    )
    return cv2.morphologyEx(yellow_mask, cv2.MORPH_CLOSE, kernel)


def extract_line_pixels(yellow_mask, center, angle, length, width):
    """把标线定向矩形范围内的【原始黄像素】抠出来，矩形外的一律不算。

    必须是原始像素，不能是填实的剪影 —— 缺了多少漆正是靠数这些像素数出来的。
    """
    half_len, half_w = length / 2.0, width / 2.0

    rect = np.array([
        [-half_len, -half_w],
        [half_len, -half_w],
        [half_len, half_w],
        [-half_len, half_w],
    ])

    rad = np.deg2rad(angle)

    rot = np.array([
        [np.cos(rad), -np.sin(rad)],
        [np.sin(rad), np.cos(rad)],
    ])

    rect = (
        np.dot(rect, rot.T) + np.asarray(center, np.float64)
    ).astype(np.int32)

    roi = np.zeros(yellow_mask.shape, np.uint8)

    cv2.fillPoly(roi, [rect], 255)

    return cv2.bitwise_and(yellow_mask, yellow_mask, mask=roi)


def evaluate_line_quality(line_mask):
    """返回 (need_paint, 说明)。line_mask 必须是原始黄像素，不是填实剪影。

    说明里带 cov/gap/w 三个数，画面直接显示，标定就靠它。
    """
    th = THRESHOLDS

    ys, xs = np.nonzero(line_mask)

    if len(xs) < th["MIN_LINE_PIXELS"]:
        return False, "No line"

    pts = np.stack([xs, ys], 1).astype(np.float32)

    rel = pts - pts.mean(0)

    # 2x2 协方差的主方向就是标线走向。用的是相对均值的坐标，
    # 所以图像坐标系怎么转都不影响结果。
    evals, evecs = np.linalg.eigh(rel.T @ rel)

    axis = evecs[:, int(np.argmax(evals))]   # 沿线方向

    t = rel @ axis

    # 只取中间一段，别让窗口无限长：越长，一段破损占的比例越小，
    # 断裂判据 MAX_GAP_RATIO 就越难触发
    span = float(t.max() - t.min())

    if span > th["MAX_ANALYSIS_LEN"]:
        mid = float(np.median(t))
        keep = np.abs(t - mid) <= th["MAX_ANALYSIS_LEN"] / 2.0
        t = t[keep]
        span = float(t.max() - t.min())

    if span < th["MIN_LINE_LENGTH"]:
        return False, "Too short"

    # 沿线逐行数黄像素，分母取"典型满行"的像素数 —— 从实际行统计里来。
    # 所以双黄线中间那 12~25px 的空隙不会被算成没漆（两条线都在，每行自然就多），
    # 线弯了也没关系（弯了也每一行都有漆）。
    t0 = float(t.min())

    rows = np.bincount((t - t0).astype(np.int32))

    full = float(np.percentile(rows[rows > 0], 75))

    if full < th["MIN_ROW_PIXELS"]:
        return False, "Too thin"

    cover = np.clip(rows / full, 0.0, 1.0)

    coverage = float(cover.mean())

    # 最长连续"没漆段"，按全长的比例算 —— 这样远近一个标准
    longest = run = 0

    for empty in cover < th["SLAB_COVERAGE_MIN"]:
        run = run + 1 if empty else 0
        longest = max(longest, run)

    gap = longest / len(cover)

    note = f"cov={coverage:.2f} gap={gap:.2f} w={full:.0f}"

    if gap > th["MAX_GAP_RATIO"]:
        return True, "Break " + note

    if coverage < th["MIN_COVERAGE"]:
        return True, "Worn " + note

    return False, "OK " + note
