#!/bin/bash
sudo apt update
sudo apt install -y python3-pyqt5 python3-serial

cat > ~/Desktop/道路画线机控制面板.sh << EOF
#!/bin/bash
export DISPLAY=:0
cd /home/$USER/marking-host
python3 main.py
EOF

chmod +x ~/Desktop/道路画线机控制面板.sh
echo "安装完成！桌面已生成启动图标"
