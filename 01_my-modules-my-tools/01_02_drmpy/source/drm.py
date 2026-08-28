#!/usr/bin/env
"""
drm.py   Gestiona EPUB en base a tu .der
Uso: drm [-h/--help] -k/--key KEY -f/--file FILE [-o/--output OUTPUT]
"""
import os
import sys
import zipfile
import tempfile
import subprocess
import xml.etree.ElementTree as ET
import base64
import re
import zlib
import gzip
import argparse
from pathlib import Path

def detect_compression(data, expected_size=None):
    """
    Detecta el método de compresión basado en los datos y tamaño esperado.
    Devuelve (datos_descomprimidos, metodo)
    """
    # 1. raw deflate (wbits=-15)
    try:
        decompressed = zlib.decompress(data, wbits=-15)
        if expected_size and len(decompressed) == expected_size:
            return decompressed, "raw_deflate"
        if not expected_size:
            return decompressed, "raw_deflate"
    except:
        pass

    # 2. zlib
    try:
        decompressed = zlib.decompress(data, wbits=15)
        if expected_size and len(decompressed) == expected_size:
            return decompressed, "zlib"
        if not expected_size:
            return decompressed, "zlib"
    except:
        pass

    # 3. gzip
    try:
        decompressed = gzip.decompress(data)
        if expected_size and len(decompressed) == expected_size:
            return decompressed, "gzip"
        if not expected_size:
            return decompressed, "gzip"
    except:
        pass

    # 4, fallback
    return data, "none"

def main():

    parser = argparse.ArgumentParser(description='Descifra EPUBs con DRM de Adobe')
    parser.add_argument('-k', '--key', required=True, help='Ruta a la clave .der')
    parser.add_argument('-f', '--file', required=True, help='Ruta al EPUB cifrado')
    parser.add_argument('-o', '--output', help='Ruta de salida (opcional)')

    args = parser.parse_args()

    if not os.path.exists(args.file):
        print(f"ERROR: No existe {args.file}")
        sys.exit(1)

    if not os.path.exists(args.key):
        print(f"ERROR: No existe {args.key}")
        sys.exit(1)

    if args.output:
        OUTPUT = args.output
    else:
        OUTPUT = os.path.splitext(args.file)[0] + "_open.epub"

    if os.path.exists(OUTPUT):
        print(f"ERROR: {OUTPUT} ya existe")
        sys.exit(1)

    print(f"\n* Descifrando: {os.path.basename(args.file)}")

    with open(args.key, "rb") as f:
        private_key_der = f.read()

    with tempfile.TemporaryDirectory() as tmpdir:

        # Extraer
        with zipfile.ZipFile(args.file, 'r') as z:

            z.extractall(tmpdir)

        # Obtener clave del libro
        rights_path = Path(tmpdir) / "META-INF" / "rights.xml"

        if not rights_path.exists():
            print("ERROR: rights.xml no encontrado")
            sys.exit(1)

        with open("/tmp/private.pem", "w") as f:
            f.write("-----BEGIN RSA PRIVATE KEY-----\n")
            b64 = base64.b64encode(private_key_der).decode('ascii')

            for i in range(0, len(b64), 64):
                f.write(b64[i:i+64] + "\n")

            f.write("-----END RSA PRIVATE KEY-----\n")

        tree = ET.parse(rights_path)
        root = tree.getroot()
        ns = {'adept': 'http://ns.adobe.com/adept'}
        enc_key = root.find('.//adept:encryptedKey', ns)

        if enc_key is None:

            print("ERROR: encryptedKey no encontrado")
            sys.exit(1)

        with open("/tmp/bookkey_enc.bin", "wb") as f:

            f.write(base64.b64decode(enc_key.text.strip()))

        r = subprocess.run([
            'openssl', 'rsautl', '-decrypt',
            '-inkey', '/tmp/private.pem',
            '-in', '/tmp/bookkey_enc.bin',
            '-out', '/tmp/bookkey_dec.bin'
        ], stderr=subprocess.DEVNULL)


        if r.returncode != 0:

            print("ERROR: Fallo al descifrar la clave")
            sys.exit(1)


        with open("/tmp/bookkey_dec.bin", "rb") as f:

            bookkey = f.read()

        if len(bookkey) != 16:

            print(f"ERROR: Clave incorrecta ({len(bookkey)} bytes)")
            sys.exit(1)

        key_hex = bookkey.hex().upper()


        enc_xml = Path(tmpdir) / "META-INF" / "encryption.xml"

        with open(enc_xml, 'r') as f:
            enc_content = f.read()

        file_sizes = {}

        for match in re.finditer(r'URI="([^"]+)".*?ResourceSize>([0-9]+)<', enc_content, re.DOTALL):

            uri = match.group(1)
            size = int(match.group(2))

            if uri and not uri.startswith('META-INF'):

                file_sizes[uri] = size

        encrypted_files = list(dict.fromkeys([

            f for f in re.findall(r'URI="([^"]+)"', enc_content)

            if f and not f.startswith('META-INF')

        ]))


        print(f"* Descifrando {len(encrypted_files)} archivos...")
        print(f"* Usando detección automática de compresión")

        ok = 0

        compression_methods = {}

        for i, file_path in enumerate(encrypted_files, 1):

            full_path = Path(tmpdir) / file_path

            if not full_path.exists():
                continue

            with open(full_path, 'rb') as f:
                data = f.read()

            if len(data) < 16:
                continue

            iv = data[:16]
            encrypted = data[16:]

            with open("/tmp/enc.bin", "wb") as f:
                f.write(encrypted)

            r = subprocess.run([
                'openssl', 'enc', '-d', '-aes-128-cbc',
                '-K', key_hex,
                '-iv', iv.hex(),
                '-in', '/tmp/enc.bin',
                '-out', '/tmp/dec.bin'
            ], stderr=subprocess.DEVNULL)

            if r.returncode != 0:

                continue

            with open("/tmp/dec.bin", "rb") as f:

                decrypted = f.read()

            expected = file_sizes.get(file_path)
            decompressed, method = detect_compression(decrypted, expected)

            if method not in compression_methods:

                compression_methods[method] = 0

            compression_methods[method] += 1

            with open(full_path, 'wb') as f:

                f.write(decompressed)

            ok += 1

            if i % 10 == 0:

                print(f"   {i}/{len(encrypted_files)}")



        # CLEANS
        (Path(tmpdir) / "META-INF" / "rights.xml").unlink(missing_ok=True)
        (Path(tmpdir) / "META-INF" / "encryption.xml").unlink(missing_ok=True)


        # Crear EPUB
        print("* Creando EPUB...")
        files = []
        for root, _, fs in os.walk(tmpdir):

            for f in fs:

                full = os.path.join(root, f)
                arc = os.path.relpath(full, tmpdir)

                if arc not in ['META-INF/rights.xml', 'META-INF/encryption.xml']:

                    files.append((full, arc))

        files.sort(key=lambda x: 0 if x[1] == 'mimetype' else 1)

        with zipfile.ZipFile(OUTPUT, 'w', zipfile.ZIP_STORED) as z:

            for full, arc in files:

                if arc == 'mimetype':
                    z.write(full, arc, zipfile.ZIP_STORED)

                else:
                    z.write(full, arc, zipfile.ZIP_DEFLATED)

    print(f"\n[OK] {OUTPUT}")
    print(f"   Tamaño: {os.path.getsize(OUTPUT):,} bytes")



if __name__ == "__main__":
    main()
