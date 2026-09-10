#!/bin/bash
export DISPLAY=:0
# 用脚本自己所在的目录，别写死（原来写死 /home/z/marking-host，换个用户就废）
cd "$(dirname "$(readlink -f "$0")")"
python3 main.py
