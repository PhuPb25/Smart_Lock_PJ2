#!/usr/bin/env python3
"""
build_index_h.py
Ghép index.html + style.css + app.js thành index.h cho ESP32 firmware.

Cách dùng:
    python build_index_h.py

Đặt script này cùng thư mục với index.html, style.css, app.js.
File index.h sẽ được tạo ra (hoặc ghi đè) trong cùng thư mục.
"""

import re
from pathlib import Path

HERE     = Path(__file__).parent
HTML_SRC = HERE / "index.html"
CSS_SRC  = HERE / "index.css"
JS_SRC   = HERE / "index.js"
OUT      = HERE / "index.h"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def build() -> str:
    html = read(HTML_SRC)
    css  = read(CSS_SRC)
    js   = read(JS_SRC)

    # 1. Thay <link rel="stylesheet" href="style.css"> bằng <style>...</style> inline
    style_block = f"<style>\n{css}\n</style>"
    html = re.sub(
        r'<link\s+rel=["\']stylesheet["\']\s+href=["\']style\.css["\'][^>]*>',
        style_block,
        html,
        flags=re.IGNORECASE,
    )

    # 2. Thay <script src="app.js"></script> bằng <script>...</script> inline
    script_block = f"<script>\n{js}\n</script>"
    html = re.sub(
        r'<script\s+src=["\']app\.js["\'][^>]*></script>',
        script_block,
        html,
        flags=re.IGNORECASE,
    )

    # 3. Bọc trong C header
    # Escape ký tự ) + rawliteral nếu có trong nội dung (cực kỳ hiếm)
    if ")rawliteral" in html:
        print("CẢNH BÁO: Nội dung HTML chứa chuỗi ')rawliteral' — có thể gây lỗi biên dịch!")

    header = f"""\
#include <Arduino.h>

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
{html}
)rawliteral";

#endif
"""
    return header


def main():
    for f in (HTML_SRC, CSS_SRC, JS_SRC):
        if not f.exists():
            print(f"LỖI: Không tìm thấy file '{f.name}' trong {HERE}")
            return

    result = build()
    OUT.write_text(result, encoding="utf-8")

    size_kb = OUT.stat().st_size / 1024
    print(f"✅ Đã tạo '{OUT.name}' ({size_kb:.1f} KB)")
    print(f"   HTML: {HTML_SRC.stat().st_size} bytes")
    print(f"   CSS:  {CSS_SRC.stat().st_size} bytes")
    print(f"   JS:   {JS_SRC.stat().st_size} bytes")


if __name__ == "__main__":
    main()