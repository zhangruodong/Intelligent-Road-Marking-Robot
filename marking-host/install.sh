#!/bin/bash
set -e

# python3-opencv 不能漏：main_window.py 第一行就是 import cv2，
# 少了它程序根本起不来（原来只装了 pyqt5 和 serial）。
sudo apt update
sudo apt install -y python3-pyqt5 python3-serial python3-opencv

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cat > ~/Desktop/道路画线机控制面板.sh << EOF
#!/bin/bash
export DISPLAY=:0
cd "$DIR"
python3 main.py
EOF

chmod +x ~/Desktop/道路画线机控制面板.sh
echo "安装完成！桌面已生成启动图标（指向 $DIR）"
