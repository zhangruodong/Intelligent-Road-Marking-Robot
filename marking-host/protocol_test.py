"""
指令表自检。跑了它才知道面板发的字节和固件认的字节对得上：

    python3 protocol_test.py     # 或 pytest protocol_test.py

不对齐的代价不是报错，是按下按钮车不动 —— 所以让它在改代码时就响。
"""

import re
from pathlib import Path

from core.protocol import COMMANDS, PANEL_BUTTONS, CAMERA_BUTTONS, Protocol

# 下位机 HandleCmd 认识的全部指令 —— 直接从固件源码里抠，不再手抄。
# 原来这里是 set("+-ADFGJKPRTUWXYZ")，手抄的，加个指令忘了同步就查不出来。
FIRMWARE_C = (
    Path(__file__).resolve().parent.parent
    / "marking-firmware" / "Hardware" / "system.c"
)

FIRMWARE_COMMANDS = set(
    re.findall(r"case '([^']+)':", FIRMWARE_C.read_text(encoding="gbk"))
)

# 下位机还没实现的指令：按下去是个空操作。写在这儿是为了让"还没做"和
# "悄悄漏了"分得开 —— 固件一补上，test_not_implemented_is_still_true 就响。
NOT_IMPLEMENTED = {"N"}


def test_panel_commands_are_known():
    """每个按钮发的字节，上位机自己得能翻译，否则日志全是"未知"。"""
    for label, cmd in PANEL_BUTTONS:
        assert len(cmd) == 1, f"{label}: 必须是单字节"
        assert cmd[0] in COMMANDS, f"{label}: {cmd!r} 不在指令表里"


def test_covers_firmware():
    """下位机能回显的每个字节都要有翻译，不然面板会显示"未知"。"""
    missing = sorted(c for c in FIRMWARE_COMMANDS if ord(c) not in COMMANDS)
    assert not missing, f"固件能回但指令表里没有: {missing}"


def test_panel_commands_reach_firmware():
    """面板发的每个字节固件都得认识 —— 不认识不是报错，是按下车不动。

    这条就是 C 当初漏掉的那条：指令表里有、按钮也在，固件 switch 里没有。
    """
    sent = {cmd[0] for _, cmd in PANEL_BUTTONS}
    known = {ord(ch) for ch in FIRMWARE_COMMANDS}
    unknown = sorted(
        chr(b) for b in sent - known if chr(b) not in NOT_IMPLEMENTED
    )
    assert not unknown, f"面板发得出去、固件不认: {unknown}"


def test_firmware_has_no_code_swallowed_by_a_comment():
    """插代码插到了行尾 // 注释里 —— 只有编译器看得见。

    实际踩过两次：system.h 枚举那个逗号落进了注释（STATE_PAINT = 'P'  //,），
    以及 system.c 里 Pump_Off(); 被插到 StepMotor_StopAll(); 前面同一行。
    两次都是「在行尾注释后面插东西」——捕获组连着注释一起匹配了。
    """
    bad = []

    for path in sorted(FIRMWARE_C.parent.glob("*.c")) + sorted(
        FIRMWARE_C.parent.glob("*.h")
    ):
        # 固件编码不一（GBK/UTF-8/纯 ASCII），这里只看 ASCII 结构，乱码无所谓
        text = path.read_bytes().decode("gbk", errors="replace")

        for lineno, line in enumerate(text.splitlines(), 1):
            if "//" not in line:
                continue

            code, comment = line.split("//", 1)

            # 整行注释不算：那是正常注释掉的旧代码，不是被吃掉的活代码
            if not code.strip():
                continue

            # 1) 前面是真代码、又不以分隔符收尾，注释里却带着 ; 或 ,
            #    —— 枚举那个逗号就是这么进注释的
            dangling = (
                code.strip()
                and not code.rstrip().endswith((",", ";", "{", "}"))
                and (";" in comment or "," in comment)
            )

            # 2) 注释里藏着一整条语句（...(...);  或  ...=...;）
            #    —— Pump_Off(); 被插到 StopAll(); 前面同一行就是这样，
            #    这种情况前面那条语句是完整的，光看它收没收尾看不出来
            buried = re.search(r"\)\s*;|=\s*[^;]*;", comment)

            if dangling or buried:
                bad.append(f"{path.name}:{lineno}  {line.strip()}")

    assert not bad, "代码被行尾注释吃掉了:\n" + "\n".join(bad)


def test_not_implemented_is_still_true():
    """固件补上 N 之后，把它从 NOT_IMPLEMENTED 里划掉，别再留个谎。"""
    done = sorted(NOT_IMPLEMENTED & FIRMWARE_COMMANDS)
    assert not done, f"固件已经实现了，从 NOT_IMPLEMENTED 里删掉: {done}"


def test_camera_buttons_exist():
    """弹摄像头的那两个标签必须真的是面板上的按钮，写错就静默不弹。"""
    labels = {label for label, _ in PANEL_BUTTONS}
    assert CAMERA_BUTTONS <= labels, f"不在面板上的按钮: {CAMERA_BUTTONS - labels}"


def test_parse_splits_stream():
    """一次收到多个字节要逐个拆开；跨包边界也不能丢。"""
    p = Protocol()
    assert p.parse(b"") == []
    assert p.parse(b"P")[0]["status"] == "画车位（标准）"
    assert [r["char"] for r in p.parse(b"PGT")] == ["P", "G", "T"]
    assert [r["char"] for r in p.parse(b"K")] == ["K"]
    assert p.parse(b"\x00")[0]["status"] == "未知"


if __name__ == "__main__":
    for name in sorted(n for n in globals() if n.startswith("test_")):
        globals()[name]()
        print("ok", name)
    print("全部通过")
