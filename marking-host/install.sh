#!/bin/bash
set -e

# 这个脚本本身别用 sudo 跑（里面的 apt 自己会提权）。用 sudo 跑的话 $USER 变成
# root，下面会把 dialout 权限加给 root、桌面图标写到 /root/Desktop。
if [ "$EUID" -eq 0 ]; then
    echo "别用 sudo 跑这个脚本（里面的 apt 自己会提权）。直接 ./install.sh" >&2
    exit 1
fi

# python3-opencv 不能漏：main_window.py 第一行就是 import cv2，
# 少了它程序根本起不来（原来只装了 pyqt5 和 serial）。
sudo apt update
sudo apt install -y python3-pyqt5 python3-serial python3-opencv

# 打开 /dev/serial0 要在 dialout 组里。树莓派默认用户本来就在，加了也无害；
# 组权限要重新登录才生效，所以记一下，最后提示。
NEED_RELOGIN=0
if id -nG "$USER" | grep -qw dialout; then
    echo "已在 dialout 组，跳过"
else
    sudo usermod -aG dialout "$USER"
    NEED_RELOGIN=1
fi

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICON="$HOME/Desktop/道路画线机控制面板.sh"

# Lite 版（没装桌面）没有 ~/Desktop，少了这行下面 cat > 会失败，
# 而 set -e 让脚本当场中断、报的错跟真正原因不相干。
mkdir -p "$HOME/Desktop"

cat > "$ICON" << EOF
#!/bin/bash
export DISPLAY=:0
cd "$DIR"
python3 main.py
EOF

chmod +x "$ICON"

# Bookworm 上桌面文件默认是"未信任"状态，双击不执行（只是打开编辑器）。
# 用 gio 打上信任标记；老版本的文件管理器没这概念，失败就算了，不影响。
gio set "$ICON" metadata::trusted true 2>/dev/null || true

echo "安装完成！桌面已生成启动图标（指向 $DIR）"
if [ "$NEED_RELOGIN" -eq 1 ]; then
    echo "刚把 $USER 加进 dialout 组，串口还没权限 —— 注销重新登录一次才生效"
fi
