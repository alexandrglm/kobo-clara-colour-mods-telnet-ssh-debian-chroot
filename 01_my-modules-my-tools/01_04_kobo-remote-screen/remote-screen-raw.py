#!/usr/bin/env python3
"""
Kobo Remote Screen target tool

Usage:
    remote-screen [--output <path>]
    python3 <PYTHON SCRIPT PATH>/remote-screen-raw.py [--output <path>]

When no flags, output is /tmp/fb0.raw.gz
"""

import os
import sys
import subprocess
import gzip
import tempfile

FB_DEVICE = "/dev/fb0"
DEFAULT_OUTPUT = "/tmp/fb0.raw.gz"


def capture_raw_compressed(output_path):
    """Copies the framebuffer and compresses it with gzip -1 (fast)"""
    try:
        # Create temporary file for the RAW data
        temp_raw = tempfile.NamedTemporaryFile(suffix='.raw', delete=False).name

        # 1. Capture framebuffer with dd
        cmd_dd = ["dd", f"if={FB_DEVICE}", f"of={temp_raw}", "bs=1M"]
        result = subprocess.run(cmd_dd, capture_output=True, text=True)

        if result.returncode != 0:

            print(f"❌ Error capturing framebuffer: {result.stderr}")
            os.unlink(temp_raw)
            return False



        # Check that the file is not empty
        if not os.path.exists(temp_raw) or os.path.getsize(temp_raw) == 0:

            print("❌ RAW file empty or not created")
            os.unlink(temp_raw)

            return False




        # 2. Compress with gzip -1 (fast, good compression)
        with open(temp_raw, 'rb') as f_in:

            with gzip.open(output_path, 'wb', compresslevel=1) as f_out:

                f_out.write(f_in.read())

        # 3. Clean up temporary file
        os.unlink(temp_raw)

        # 4. Check that the compressed file is not empty
        if os.path.exists(output_path) and os.path.getsize(output_path) > 0:
            raw_size = os.path.getsize(temp_raw) if os.path.exists(temp_raw) else 0
            gz_size = os.path.getsize(output_path)
            ratio = (1 - gz_size / raw_size) * 100 if raw_size > 0 else 0
            print(f"✅ Compressed RAW: {output_path} ({gz_size} bytes, {ratio:.1f}% smaller)")
            return True
        else:
            print("❌ Error: compressed file is empty")
            return False

    except Exception as e:
        print(f"❌ Error: {e}")
        # Clean up temporary files
        try:
            if 'temp_raw' in locals() and os.path.exists(temp_raw):
                os.unlink(temp_raw)
        except:
            pass
        return False


def main():
    output_path = DEFAULT_OUTPUT

    # Parse arguments
    if len(sys.argv) > 1:

        if sys.argv[1] in ['-h', '--help']:

            print("Usage: remote-screen-raw.py [--output <path>]")
            print(f"Default: {DEFAULT_OUTPUT}")
            print("Note: The file is saved compressed with gzip")

            sys.exit(0)

        if sys.argv[1] == '--output' and len(sys.argv) > 2:

            output_path = sys.argv[2]


            # Ensure it ends with .gz
            if not output_path.endswith('.gz'):

                output_path += '.gz'


    # Ensure the directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)


    # Capture and compress
    if capture_raw_compressed(output_path):

        sys.exit(0)

    else:

        sys.exit(1)


if __name__ == "__main__":
    main()
