#!/bin/bash
export DISPLAY=:0
# 用脚本自己所在的目录，别写死绝对路径（换个用户名、或把仓库克隆到别处就废）
cd "$(dirname "$(readlink -f "$0")")"
python3 main.py
