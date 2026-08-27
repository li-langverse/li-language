#!/usr/bin/env python3
"""Convert PPM frames to PNG (stdlib only) for studio native capture."""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


def ppm_to_png(ppm: Path, png: Path) -> None:
    data = ppm.read_bytes()
    if not data.startswith(b"P6\n"):
        raise ValueError(f"not P6 PPM: {ppm}")
    _magic, dims, rest = data.split(b"\n", 2)
    maxval_line, rgb = rest.split(b"\n", 1)
    w_s, h_s = dims.split()
    w, h = int(w_s), int(h_s)
    if maxval_line != b"255":
        raise ValueError(f"expected maxval 255, got {maxval_line!r}: {ppm}")
    if len(rgb) != w * h * 3:
        raise ValueError(f"size mismatch {ppm}")
    # PNG RGB
    raw = b"".join(b"\x00" + rgb[y * w * 3 : (y + 1) * w * 3] for y in range(h))
    def chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    idat = zlib.compress(raw, 9)
    blob = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", idat)
        + chunk(b"IEND", b"")
    )
    png.write_bytes(blob)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ppm_dir", type=Path)
    ap.add_argument("png_dir", type=Path)
    args = ap.parse_args()
    args.png_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for ppm in sorted(args.ppm_dir.glob("frame-*.ppm")):
        out = args.png_dir / (ppm.stem + ".png")
        ppm_to_png(ppm, out)
        count += 1
    if count == 0:
        print("studio-ppm-to-png: no frame-*.ppm found", flush=True)
        return 1
    print(f"studio-ppm-to-png: wrote {count} PNG(s) → {args.png_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
