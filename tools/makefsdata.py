#!/usr/bin/env python3
"""Generate lib/httpd/fsdata.c from the static files in www/.

Produces the same on-disk format as the original makefsdata.js (the lwIP
fsdata layout with the filename, HTTP headers and payload concatenated per
file), so the httpd library can serve it unchanged. Uses raw zlib deflate
for compressible types, matching the old Node build.
"""

import argparse
import math
import os
import re
import sys
import zlib
from pathlib import Path

SERVER_HEADER = "MP2040"

CONTENT_TYPES = {
    "html": "text/html",
    "htm": "text/html",
    "shtml": 'text/html\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache',
    "shtm": 'text/html\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache',
    "ssi": 'text/html\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache',
    "gif": "image/gif",
    "png": "image/png",
    "jpg": "image/jpeg",
    "jpeg": "image/jpeg",
    "bmp": "image/bmp",
    "ico": "image/x-icon",
    "class": "application/octet-stream",
    "cls": "application/octet-stream",
    "js": "application/javascript",
    "ram": "application/javascript",
    "css": "text/css",
    "swf": "application/x-shockwave-flash",
    "xml": "text/xml",
    "xsl": "text/xml",
    "pdf": "application/pdf",
    "json": "application/json",
    "svg": "image/svg+xml",
}

DEFAULT_CONTENT_TYPE = "text/plain"

# These extensions were not compressed by the original makefsdata
NO_COMPRESS = {"png", "json", "svg"}

PAYLOAD_ALIGNMENT = 4
HEX_BYTES_PER_LINE = 16


def c_string_length(s: str) -> int:
    return len(s.encode("utf-8"))


def fix_filename_for_c(name: str) -> str:
    return re.sub(r"[^a-zA-Z0-9]", "_", name)


def hex_bytes(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), HEX_BYTES_PER_LINE):
        chunk = data[i : i + HEX_BYTES_PER_LINE]
        lines.append(",".join(f"0x{b:02x}" for b in chunk) + ",")
    return "\n".join(lines) + "\n"


def c_string(data: str) -> str:
    return hex_bytes(data.encode("utf-8"))


def gather_files(web_dir: Path):
    files = []
    for root, dirs, names in os.walk(web_dir):
        dirs[:] = [d for d in dirs if not d.startswith(".") and d != "__pycache__"]
        for name in sorted(names):
            if name.startswith("."):
                continue
            files.append(Path(root) / name)
    return sorted(files)


def makefsdata(web_dir: Path, out_file: Path, board_svg: str = ""):
    # List of (absolute path, URL path) pairs
    entries = [(p, "/" + p.relative_to(web_dir).as_posix()) for p in gather_files(web_dir)]
    if board_svg and Path(board_svg).is_file():
        entries.append((Path(board_svg), "/board.svg"))
    entries.sort(key=lambda e: e[1])
    fsdata = []
    fsdata.append('#include "fsdata.h"')
    fsdata.append("")
    fsdata.append("#define file_NULL (struct fsdata_file *) NULL")
    fsdata.append("")
    fsdata.append("#ifndef FS_FILE_FLAGS_HEADER_INCLUDED")
    fsdata.append("#define FS_FILE_FLAGS_HEADER_INCLUDED 1")
    fsdata.append("#endif")
    fsdata.append("#ifndef FS_FILE_FLAGS_HEADER_PERSISTENT")
    fsdata.append("#define FS_FILE_FLAGS_HEADER_PERSISTENT 0")
    fsdata.append("#endif")
    fsdata.append("/* FSDATA_FILE_ALIGNMENT: 0=off, 1=by variable, 2=by include */")
    fsdata.append("#ifndef FSDATA_FILE_ALIGNMENT")
    fsdata.append("#define FSDATA_FILE_ALIGNMENT 0")
    fsdata.append("#endif")
    fsdata.append("#ifndef FSDATA_ALIGN_PRE")
    fsdata.append("#define FSDATA_ALIGN_PRE")
    fsdata.append("#endif")
    fsdata.append("#ifndef FSDATA_ALIGN_POST")
    fsdata.append("#define FSDATA_ALIGN_POST")
    fsdata.append("#endif")
    fsdata.append("#if FSDATA_FILE_ALIGNMENT==2")
    fsdata.append('#include "fsdata_alignment.h"')
    fsdata.append("#endif")
    fsdata.append("")

    file_infos = []
    payload_alignment_counter = 0

    for file_path, rel in entries:
        ext = rel.split(".")[-1].lower()
        var_name = fix_filename_for_c(rel)

        fsdata.append("#if FSDATA_FILE_ALIGNMENT==1")
        fsdata.append(f"static const unsigned int dummy_align_{var_name} = {payload_alignment_counter};")
        fsdata.append("#endif")
        payload_alignment_counter += 1

        raw = file_path.read_bytes()

        is_compressed = False
        payload = raw
        if ext not in NO_COMPRESS:
            compressed = zlib.compress(raw, 9)
            if len(compressed) < len(raw):
                payload = compressed
                is_compressed = True
                print(f"Compressed {rel} from {len(raw)} to {len(compressed)} bytes")
            else:
                print(f"Skipping compression of {rel}, compressed size is larger than original")
        else:
            print(f"Skipping compression of {rel} by file extension")

        name_len = c_string_length(rel) + 1
        padded_name_len = math.ceil(name_len / PAYLOAD_ALIGNMENT) * PAYLOAD_ALIGNMENT
        padded_name = rel.encode("utf-8") + b"\0" * (1 + padded_name_len - name_len)

        header_parts = []
        header_parts.append(b"HTTP/1.0 200 OK\r\n")
        header_parts.append(f"Server: {SERVER_HEADER}\r\n".encode("utf-8"))
        header_parts.append(f"Content-Length: {len(payload)}\r\n".encode("utf-8"))
        if is_compressed:
            header_parts.append(b"Content-Encoding: deflate\r\n")
        header_parts.append(f"Content-Type: {CONTENT_TYPES.get(ext, DEFAULT_CONTENT_TYPE)}\r\n\r\n".encode("utf-8"))
        header = b"".join(header_parts)

        fsdata.append(f"static const unsigned char data_{var_name}[] FSDATA_ALIGN_PRE = {{")
        fsdata.append(f"/* {rel} ({name_len} chars) */")
        fsdata.append(hex_bytes(padded_name))
        fsdata.append("")
        fsdata.append("/* HTTP header */")
        fsdata.append(c_string("HTTP/1.0 200 OK\r\n"))
        fsdata.append(c_string(f"Server: {SERVER_HEADER}\r\n"))
        fsdata.append(c_string(f"Content-Length: {len(payload)}\r\n"))
        if is_compressed:
            fsdata.append(c_string("Content-Encoding: deflate\r\n"))
        fsdata.append(c_string(f"Content-Type: {CONTENT_TYPES.get(ext, DEFAULT_CONTENT_TYPE)}\r\n\r\n"))
        fsdata.append(f"/* raw file data ({len(payload)} bytes) */")
        fsdata.append(hex_bytes(payload))
        fsdata.append("};")
        fsdata.append("")

        file_infos.append((var_name, padded_name_len, ext))

    prev = "NULL"
    for var_name, padded_name_len, ext in file_infos:
        is_ssi = ext in {"shtml", "shtm", "ssi", "xml", "json"}
        fsdata.append(f"const struct fsdata_file file_{var_name}[] = {{")
        fsdata.append(f"file_{prev},")
        fsdata.append(f"data_{var_name},")
        fsdata.append(f"data_{var_name} + {padded_name_len},")
        fsdata.append(f"sizeof(data_{var_name}) - {padded_name_len},")
        if is_ssi:
            fsdata.append("FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_SSI")
        else:
            fsdata.append("FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT")
        fsdata.append("};")
        fsdata.append("")
        prev = var_name

    fsdata.append(f"#define FS_ROOT file_{prev}")
    fsdata.append(f"#define FS_NUMFILES {len(file_infos)}")
    fsdata.append("")

    out_file.write_text("\n".join(fsdata), encoding="utf-8")
    print(f"Wrote {out_file} ({len(file_infos)} files)")


def main():
    parser = argparse.ArgumentParser(description="Generate lwIP fsdata.c from static web files")
    parser.add_argument("web_dir", type=Path, help="Directory containing the static web files")
    parser.add_argument("out_file", type=Path, help="Output path for fsdata.c")
    parser.add_argument("--board-svg", nargs="?", const="", default="",
                        help="Optional board.svg to embed as /board.svg")
    args = parser.parse_args()

    if not args.web_dir.is_dir():
        print(f"Error: {args.web_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    makefsdata(args.web_dir, args.out_file, args.board_svg)


if __name__ == "__main__":
    main()
