#!/usr/bin/env python3
"""
screenshot.py   Captura framebuffer y guarda a PNG sin deps
Uso: screenshot [--output nombre] [--help]
"""

import os
import sys
import time
import subprocess
import struct
import zlib

WRAPPER = "/usr/bin/screenshot"
FB_DEVICE = "/dev/fb0"
SCREENSHOT_DIR = "/mnt/onboard/screenshots"
WIDTH = 1072
HEIGHT = 1448


def check_wrapper(WRAPPER):

    if os.path.exists(WRAPPER) and os.access(WRAPPER, os.X_OK):
        return True

    print(f"⚠  Wrapper not found or not executable: {WRAPPER}")

    wrapper_content = '''#!/bin/sh
export PYTHONHOME=/mnt/onboard/.python/usr
export PYTHONPATH=/mnt/onboard/.python/usr/lib/python3.11
export LD_LIBRARY_PATH=/mnt/onboard/.python/usr/lib:$LD_LIBRARY_PATH
exec /mnt/onboard/.python/usr/bin/python3.11 /mnt/onboard/.python/screenshot.py "$@"
'''

    try:

        with open(WRAPPER, 'w') as f:

            f.write(wrapper_content)

        os.chmod(WRAPPER, 0o755)

        print(f"✅ Wrapper created: {WRAPPER}")
        return True

    except Exception as e:

        print(f"❌ Failed to create wrapper: {e}")
        return False

def check_framebuffer(device=FB_DEVICE):

    if not os.path.exists(device):

        print(f"❌ Framebuffer device not found: {device}")
        print(f"ℹ  Please specify your framebuffer device with --fb-device")
        print(f"   Example: screenshot --fb-device /dev/fb1")

        return False

    if not os.access(device, os.R_OK):

        print(f"❌ Framebuffer device not readable: {device}")
        return False

    return True

def check_paths():

    os.makedirs(SCREENSHOT_DIR, exist_ok=True)

def get_timestamp():
    return time.strftime("%Y%m%d_%H%M%S")

def get_png_path(custom_name=None):

    if custom_name:

        if not custom_name.endswith('.png'):

            custom_name += '.png'

        return f"{SCREENSHOT_DIR}/{custom_name}"

    return f"{SCREENSHOT_DIR}/screenshot_{get_timestamp()}.png"

def capture_raw(output_path):

    try:

        cmd = ["dd", f"if={FB_DEVICE}", f"of={output_path}", "bs=1M"]
        subprocess.run(cmd, check=True, capture_output=True)

        if not os.path.exists(output_path) or os.path.getsize(output_path) == 0:

            return False

        return True

    except:

        return False

def raw_to_png(raw_path, png_path):

    with open(raw_path, 'rb') as f:
        data = f.read()

    # BGRA -> RGB
    rgb = bytearray()

    for i in range(0, len(data), 4):

        if i + 4 > len(data):

            break

        b, g, r, a = data[i:i+4]

        rgb.extend((r, g, b))

    # PNG
    def png_chunk(chunk_type, data):

        length = len(data)

        crc = zlib.crc32(chunk_type + data) & 0xffffffff

        return struct.pack('>I', length) + chunk_type + data + struct.pack('>I', crc)

    header = b'\x89PNG\r\n\x1a\n'

    ihdr = png_chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))

    scanlines = bytearray()

    for y in range(HEIGHT):

        scanlines.append(0)

        scanlines.extend(rgb[y*WIDTH*3:(y+1)*WIDTH*3])

    idat = png_chunk(b'IDAT', zlib.compress(scanlines, 9))

    iend = png_chunk(b'IEND', b'')

    with open(png_path, 'wb') as f:

        f.write(header + ihdr + idat + iend)

    return True

def handle_raw_capture(path):

    try:

        if os.path.exists(path):
            os.remove(path)
            return True

    except Exception as e:

        print(f"Error removing RAW screenshot: {e}")
        return False

    return False

def main():

    output_name = None

    if not check_wrapper(WRAPPER):
        sys.exit(1)


    if not check_framebuffer():
        sys.exit(1)


    if len(sys.argv) > 1:

        if sys.argv[1] in ['-h', '--help']:

            print("Usage:   screenshot [--output <filename>]")
            sys.exit(0)

        if sys.argv[1] == '--output' and len(sys.argv) > 2:

            output_name = sys.argv[2]

    check_paths()

    raw_path = f"/tmp/raw_{get_timestamp()}.raw"

    png_path = get_png_path(output_name)

    # print(f"📸 Capturando screenshot...")

    if not capture_raw(raw_path):

        print("❌ Error capturando framebuffer")
        sys.exit(1)


    # print(f"🔄 Convirtiendo a PNG...")
    if not raw_to_png(raw_path, png_path):

        print("❌ Error convirtiendo a PNG")
        handle_raw_capture(raw_path)

        sys.exit(1)

    handle_raw_capture(raw_path)
    print(f"✅ Screenshot saved at: {png_path}")



if __name__ == "__main__":
    main()

