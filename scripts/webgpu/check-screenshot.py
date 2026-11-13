#!/usr/bin/env python3
"""Checks the on-glass screenshot of rnqnx_webgpu_test.

The test window sits at (120, 90), 640x480. It holds a red triangle over a
blue clear colour whose green channel animates between 51 and 128. The Screen
`screenshot` tool writes a BMP of the whole display. This reads that BMP with
the standard library only, then checks that

  * the pixel at the window centre is the triangle's red, and
  * a pixel just inside the window's top-left corner is blue-ish (blue clearly
    greater than red), so the window shows the WebGPU frame and not the desktop
    behind it. One sample cannot show the animation; the Qt compose test
    compares two grabs for that.

Usage: check-screenshot.py <screen.bmp> [x y width height]
Exit status 0 when both checks hold.
"""
import struct
import sys


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("not a BMP file")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bpp, compression = struct.unpack_from("<iihhI", data, 18)
    if compression not in (0, 3) or bpp not in (24, 32):
        raise ValueError(f"unsupported BMP: bpp={bpp} compression={compression}")
    top_down = height < 0
    height = abs(height)
    stride = (width * bpp // 8 + 3) // 4 * 4
    bytes_per_pixel = bpp // 8

    def pixel(x, y):
        row = y if top_down else height - 1 - y
        i = offset + row * stride + x * bytes_per_pixel
        b, g, r = data[i], data[i + 1], data[i + 2]
        return r, g, b

    return width, height, pixel


def main():
    if len(sys.argv) not in (2, 6):
        print(__doc__)
        return 2
    wx, wy, ww, wh = (120, 90, 640, 480)
    if len(sys.argv) == 6:
        wx, wy, ww, wh = (int(v) for v in sys.argv[2:6])
    width, height, pixel = read_bmp(sys.argv[1])
    print(f"screenshot {width}x{height}")
    cx, cy = wx + ww // 2, wy + wh // 2
    ix, iy = wx + 6, wy + 6
    if not (0 <= ix < width and 0 <= cy < height):
        print("window is outside the screenshot")
        return 1
    center = pixel(cx, cy)
    inner = pixel(ix, iy)
    print(f"window centre ({cx},{cy}) rgb={center}")
    print(f"window inner corner ({ix},{iy}) rgb={inner}")
    red_center = center[0] > 200 and center[1] < 40 and center[2] < 40
    blue_corner = inner[2] > inner[0] + 60
    print(f"triangle red at centre: {red_center}")
    print(f"clear colour at inner corner: {blue_corner}")
    return 0 if red_center and blue_corner else 1


if __name__ == "__main__":
    sys.exit(main())
