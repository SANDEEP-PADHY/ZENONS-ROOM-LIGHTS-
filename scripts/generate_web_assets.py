import os
import gzip
import shutil

WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")
DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
INCLUDE_DIR = os.path.join(os.path.dirname(__file__), "..", "firmware", "include")

def build_assets():
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(os.path.join(DATA_DIR, "css"), exist_ok=True)
    os.makedirs(os.path.join(DATA_DIR, "js"), exist_ok=True)
    os.makedirs(os.path.join(DATA_DIR, "room"), exist_ok=True)
    os.makedirs(INCLUDE_DIR, exist_ok=True)

    files_to_pack = [
        ("index.html", "index.html", "text/html"),
        (os.path.join("css", "style.css"), "css/style.css", "text/css"),
        (os.path.join("js", "app.js"), "js/app.js", "application/javascript"),
        (os.path.join("room", "room.svg"), "room/room.svg", "image/svg+xml"),
    ]

    header_content = """#pragma once
#include <Arduino.h>

// Auto-generated embedded web assets (gzipped) for instant offline serving
"""

    for src_rel, dest_rel, mime in files_to_pack:
        src_path = os.path.join(WEB_DIR, src_rel)
        dest_path = os.path.join(DATA_DIR, dest_rel)

        if not os.path.exists(src_path):
            print(f"Warning: {src_path} not found!")
            continue

        with open(src_path, "rb") as f_in:
            data = f_in.read()

        # Copy raw uncompressed to data/
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        with open(dest_path, "wb") as f_out:
            f_out.write(data)

        # Also compress to gzip for embedded header
        compressed = gzip.compress(data, compresslevel=9)
        var_name = dest_rel.replace("/", "_").replace(".", "_").replace("-", "_")

        header_content += f"\n// {dest_rel} ({len(data)} bytes raw, {len(compressed)} bytes gzipped)\n"
        header_content += f"const char {var_name}_mime[] PROGMEM = \"{mime}\";\n"
        header_content += f"const size_t {var_name}_len = {len(compressed)};\n"
        header_content += f"const uint8_t {var_name}_gz[] PROGMEM = {{\n  "

        for i, b in enumerate(compressed):
            header_content += f"0x{b:02x}, "
            if (i + 1) % 16 == 0:
                header_content += "\n  "

        header_content = header_content.rstrip(", \n") + "\n};\n"

    header_file = os.path.join(INCLUDE_DIR, "WebAssets.h")
    with open(header_file, "w", encoding="utf-8") as f_out:
        f_out.write(header_content)

    print(f"Generated {header_file} and synced {DATA_DIR} successfully.")

if __name__ == "__main__":
    build_assets()
