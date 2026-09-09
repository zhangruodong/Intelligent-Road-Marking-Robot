import os

os.environ.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
os.environ.pop("QT_PLUGIN_PATH", None)
import sys
from PyQt5.QtWidgets import QApplication
from core.serial_manager import SerialManager
from core.protocol import Protocol
from ui.main_window import MainWindow

def main():
    app = QApplication(sys.argv)
    serial_mgr = SerialManager("/dev/ttyS0", 9600)
    protocol = Protocol()
    window = MainWindow(serial_mgr, protocol)
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()