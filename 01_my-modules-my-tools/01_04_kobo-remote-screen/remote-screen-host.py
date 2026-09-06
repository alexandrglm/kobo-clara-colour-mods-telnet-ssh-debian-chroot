#!/usr/bin/env python3
"""
Kobo Remote Screen Host tool

Usage:

    export KOBO_PASS="ssh pass"
    python3 remote-screen-host.py <KOBO's IP>

"""

import sys
import os
import struct
import zlib
import gzip
import subprocess
import shutil
import tempfile

from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QPushButton, QLabel, QStatusBar,
                             QFileDialog, QSpinBox, QShortcut)
from PyQt5.QtCore import Qt, QTimer, QThread, pyqtSignal
from PyQt5.QtGui import QPixmap, QKeySequence

# Kobo settings
KOBO_IP = "192.168.1.100"
KOBO_USER = "root"
KOBO_PASS = ""

# Framebuffer dimensions
FB_WIDTH = 1072
FB_HEIGHT = 1448


def raw_to_png(raw_data, png_path):
    """Converts BGRA RAW data to PNG (fast, on host)"""
    try:
        # BGRA -> RGB
        rgb = bytearray()
        for i in range(0, len(raw_data), 4):
            if i + 4 > len(raw_data):
                break
            b, g, r, a = raw_data[i:i+4]
            rgb.extend((b, g, r))  # BGR → RGB

        def png_chunk(chunk_type, data):
            length = len(data)
            crc = zlib.crc32(chunk_type + data) & 0xffffffff
            return struct.pack('>I', length) + chunk_type + data + struct.pack('>I', crc)

        header = b'\x89PNG\r\n\x1a\n'
        ihdr = png_chunk(b'IHDR', struct.pack('>IIBBBBB', FB_WIDTH, FB_HEIGHT, 8, 2, 0, 0, 0))

        scanlines = bytearray()
        for y in range(FB_HEIGHT):
            scanlines.append(0)
            scanlines.extend(rgb[y*FB_WIDTH*3:(y+1)*FB_WIDTH*3])

        idat = png_chunk(b'IDAT', zlib.compress(scanlines, 9))
        iend = png_chunk(b'IEND', b'')

        os.makedirs(os.path.dirname(png_path), exist_ok=True)
        with open(png_path, 'wb') as f:
            f.write(header + ihdr + idat + iend)

        return True
    except Exception as e:
        print(f"Error converting RAW to PNG: {e}")
        return False


class ScreenshotThread(QThread):

    finished = pyqtSignal(str)
    error = pyqtSignal(str)

    def __init__(self, kobo_ip, user, password=""):
        super().__init__()
        self.kobo_ip = kobo_ip
        self.user = user
        self.password = password

    def run(self):
        try:

            remote_raw_gz = "/tmp/fb0.raw.gz"
            local_raw_gz = tempfile.NamedTemporaryFile(suffix='.raw.gz', delete=False).name
            local_png = "/tmp/kobo_screen.png"

            cmd = self._build_ssh_command(f"/usr/bin/remote-screen --output {remote_raw_gz}")
            result = subprocess.run(cmd, capture_output=True, text=True)

            if result.returncode != 0:
                self.error.emit(f"Error on Kobo: {result.stderr}")
                return

            scp_cmd = self._build_scp_command(remote_raw_gz, local_raw_gz)
            result = subprocess.run(scp_cmd, capture_output=True, text=True)

            if result.returncode != 0:
                self.error.emit(f"SCP Error: {result.stderr}")
                return

            with gzip.open(local_raw_gz, 'rb') as f:
                raw_data = f.read()

            if not raw_to_png(raw_data, local_png):
                self.error.emit("Error converting RAW to PNG")
                return

            try:
                os.unlink(local_raw_gz)
            except:
                pass

            self.finished.emit(local_png)

        except Exception as e:
            self.error.emit(f"Error: {str(e)}")

    def _build_ssh_command(self, command):
        cmd = ["ssh", "-o", "StrictHostKeyChecking=no"]
        if self.password:
            cmd = ["sshpass", "-p", self.password] + cmd
        cmd.extend([f"{self.user}@{self.kobo_ip}", command])
        return cmd

    def _build_scp_command(self, remote_path, local_path):
        cmd = ["scp", "-o", "StrictHostKeyChecking=no"]
        if self.password:
            cmd = ["sshpass", "-p", self.password] + cmd
        cmd.append(f"{self.user}@{self.kobo_ip}:{remote_path}")
        cmd.append(local_path)
        return cmd


class ScreenshotViewer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Kobo Remote Screen Host Tool v0.1")
        self.setMinimumSize(800, 600)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        toolbar = QHBoxLayout()

        self.btn_capture = QPushButton("Screenshot")
        self.btn_capture.clicked.connect(self.capture_screenshot)
        toolbar.addWidget(self.btn_capture)

        self.btn_autocapture = QPushButton("Auto Mode")
        self.btn_autocapture.setCheckable(True)
        self.btn_autocapture.toggled.connect(self.toggle_autocapture)
        toolbar.addWidget(self.btn_autocapture)

        toolbar.addWidget(QLabel("Interval (s):"))
        self.spin_interval = QSpinBox()
        self.spin_interval.setRange(1, 60)
        self.spin_interval.setValue(2)
        toolbar.addWidget(self.spin_interval)

        self.btn_save = QPushButton("Save")
        self.btn_save.clicked.connect(self.save_image)
        toolbar.addWidget(self.btn_save)

        toolbar.addStretch()
        layout.addLayout(toolbar)

        # Main layout
        self.image_label = QLabel()
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setStyleSheet("""
            QLabel {
                background-color: #1a1a1a;
                border: 1px solid #333;
                min-height: 400px;
            }
        """)
        self.image_label.setText("Waiting...\nPress 'Screenshot' or F5")
        layout.addWidget(self.image_label)

        # Statusbar layer
        self.statusBar = QStatusBar()
        self.setStatusBar(self.statusBar)
        self.statusBar.showMessage("Ready")

        # Timer
        self.autocapture_timer = QTimer()
        self.autocapture_timer.timeout.connect(self.capture_screenshot)

        # Keyboard shortcuts
        QShortcut(QKeySequence("Ctrl+C"), self, self.capture_screenshot)
        QShortcut(QKeySequence("F5"), self, self.capture_screenshot)
        QShortcut(QKeySequence("Ctrl+S"), self, self.save_image)

        self.current_image_path = None
        self.capturing = False

        QTimer.singleShot(500, self.capture_screenshot)

    def capture_screenshot(self):
        if self.capturing:
            return

        self.capturing = True
        self.btn_capture.setEnabled(False)
        self.statusBar.showMessage("Capturing...")

        # Solo mostramos el texto si NO hay una imagen cargada previamente
        if not self.current_image_path or not os.path.exists(self.current_image_path):
            self.image_label.setText("Capturing...")

        self.thread = ScreenshotThread(KOBO_IP, KOBO_USER, KOBO_PASS)
        self.thread.finished.connect(self.on_screenshot_ready)
        self.thread.error.connect(self.on_screenshot_error)
        self.thread.start()

    def on_screenshot_ready(self, image_path):
        self.current_image_path = image_path
        self.display_image(image_path)
        self.capturing = False
        self.btn_capture.setEnabled(True)
        self.statusBar.showMessage("Screenshot captured!")

    def on_screenshot_error(self, error_msg):
        self.capturing = False
        self.btn_capture.setEnabled(True)
        self.statusBar.showMessage(f"Error: {error_msg}")

        # Solo mostramos el texto de error si no hay imagen en pantalla
        if not self.current_image_path or not os.path.exists(self.current_image_path):
            self.image_label.setText(f"Error:\n{error_msg}")
        print(f"ERROR: {error_msg}")

    def display_image(self, image_path):
        if not os.path.exists(image_path):
            self.image_label.setText("Error: File not found")
            return

        pixmap = QPixmap(image_path)
        if not pixmap.isNull():
            scaled = pixmap.scaled(
                self.image_label.width() - 10,
                self.image_label.height() - 10,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation
            )
            self.image_label.setPixmap(scaled)
            self.image_label.setText("")
        else:
            self.image_label.setText("Error loading image")

    def toggle_autocapture(self, checked):
        if checked:
            interval = self.spin_interval.value() * 1000
            self.autocapture_timer.start(interval)
            self.btn_autocapture.setText("- Stop auto")
            self.statusBar.showMessage(f"Auto-capture enabled ({interval//1000}s)")
        else:
            self.autocapture_timer.stop()
            self.btn_autocapture.setText("Auto-capture")
            self.statusBar.showMessage("Auto-capture disabled")

    def save_image(self):
        if not self.current_image_path or not os.path.exists(self.current_image_path):
            self.statusBar.showMessage("No image to save")
            return

        import time
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save image",
            f"kobo_screenshot_{time.strftime('%Y%m%d_%H%M%S')}.png",
            "PNG Image (*.png);;All Files (*.*)"
        )

        if file_path:
            shutil.copy2(self.current_image_path, file_path)
            self.statusBar.showMessage(f"Image saved at: {os.path.basename(file_path)}")

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.current_image_path and os.path.exists(self.current_image_path):
            self.display_image(self.current_image_path)

    def closeEvent(self, event):
        self.autocapture_timer.stop()
        event.accept()


def main():
    global KOBO_IP, KOBO_PASS

    if len(sys.argv) > 1:
        KOBO_IP = sys.argv[1]
    elif "KOBO_IP" in os.environ:
        KOBO_IP = os.environ["KOBO_IP"]

    if len(sys.argv) > 2:
        KOBO_PASS = sys.argv[2]
    elif "KOBO_PASS" in os.environ:
        KOBO_PASS = os.environ["KOBO_PASS"]

    print(f"Connecting to Kobo at: {KOBO_IP}")
    print("F5 to capture | Ctrl+S to save | Ctrl+C to exit")
    print(f"Authentication: {'with password' if KOBO_PASS else 'without password'}")
    print("Capture: Compressed RAW on Kobo → PNG on host")

    app = QApplication(sys.argv)
    app.setApplicationName("Kobo Remote Screen Tool v.0.1")

    viewer = ScreenshotViewer()
    viewer.show()

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
