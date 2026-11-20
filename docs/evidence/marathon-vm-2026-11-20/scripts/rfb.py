#!/usr/bin/env python3
"""Minimal RFB client for Qt's vnc QPA plugin: pointer, key and screenshot.

usage: rfb.py [host:port] cmd [args] ...   commands run in order
  shot FILE.png | tap X Y | down X Y | move X Y | up X Y | drag X1 Y1 X2 Y2 [STEPS]
  wheel X Y up|down N | key NAME (enter, escape, back, backspace, a, ...) | type TEXT | wait SECONDS
"""
import socket, struct, sys, time, zlib

KEYSYMS = {"enter": 0xFF0D, "escape": 0xFF1B, "backspace": 0xFF08, "tab": 0xFF09,
           "back": 0x1008FF26, "forward": 0x1008FF27, "left": 0xFF51, "up": 0xFF52,
           "right": 0xFF53, "down": 0xFF54, "alt": 0xFFE9}


def recvn(s, n):
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise EOFError
        buf += chunk
    return buf


class Rfb:
    def __init__(self, addr="127.0.0.1:5900"):
        host, port = addr.split(":")
        self.s = socket.create_connection((host, int(port)), timeout=20)
        recvn(self.s, 12)
        # Qt's vnc plugin speaks RFB 3.3: the server picks the security type.
        self.s.sendall(b"RFB 003.003\n")
        assert struct.unpack(">I", recvn(self.s, 4))[0] == 1  # security: none
        self.s.sendall(b"\x01")
        self.w, self.h = struct.unpack(">HH", recvn(self.s, 4))
        recvn(self.s, 16)  # server pixel format
        (nl,) = struct.unpack(">I", recvn(self.s, 4))
        recvn(self.s, nl)
        # 32 bpp little endian BGRX, raw only.
        self.s.sendall(struct.pack(">BxxxBBBBHHHBBBxxx", 0, 32, 24, 0, 1, 255, 255, 255, 16, 8, 0))
        self.s.sendall(struct.pack(">BxHi", 2, 1, 0))
        self.mask = 0
        self.fb = bytearray(self.w * self.h * 4)

    def pointer(self, x, y, mask):
        self.mask = mask
        self.s.sendall(struct.pack(">BBHH", 5, mask, x, y))

    def key(self, sym, down):
        self.s.sendall(struct.pack(">BBxxI", 4, 1 if down else 0, sym))

    def update(self):
        self.s.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, self.w, self.h))
        while True:
            t = recvn(self.s, 1)[0]
            if t != 0:
                raise RuntimeError("unexpected message %d" % t)
            recvn(self.s, 1)
            (n,) = struct.unpack(">H", recvn(self.s, 2))
            for _ in range(n):
                x, y, w, h, enc = struct.unpack(">HHHHi", recvn(self.s, 12))
                assert enc == 0, enc
                data = recvn(self.s, w * h * 4)
                for row in range(h):
                    o = ((y + row) * self.w + x) * 4
                    self.fb[o:o + w * 4] = data[row * w * 4:(row + 1) * w * 4]
            return

    def shot(self, path):
        time.sleep(0.6)
        self.update()
        raw = bytearray()
        for row in range(self.h):
            raw.append(0)
            line = self.fb[row * self.w * 4:(row + 1) * self.w * 4]
            rgb = bytearray(self.w * 3)
            rgb[0::3] = line[2::4]
            rgb[1::3] = line[1::4]
            rgb[2::3] = line[0::4]
            raw += rgb

        def chunk(tag, body):
            c = struct.pack(">I", len(body)) + tag + body
            return c + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)) \
            + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
        open(path, "wb").write(png)
        print("shot", path, self.w, self.h)


def main():
    a = sys.argv[1:]
    addr = "127.0.0.1:5900"
    if a and ":" in a[0] and a[0][0].isdigit():
        addr, a = a[0], a[1:]
    c = Rfb(addr)
    i = 0
    while i < len(a):
        cmd = a[i]
        if cmd == "shot":
            c.shot(a[i + 1]); i += 2
        elif cmd == "tap":
            x, y = int(a[i + 1]), int(a[i + 2])
            c.pointer(x, y, 0); time.sleep(0.05); c.pointer(x, y, 1); time.sleep(0.1); c.pointer(x, y, 0); i += 3
        elif cmd in ("down", "move", "up"):
            x, y = int(a[i + 1]), int(a[i + 2])
            c.pointer(x, y, 0 if cmd == "up" else 1); i += 3
        elif cmd == "drag":
            x1, y1, x2, y2 = (int(v) for v in a[i + 1:i + 5])
            steps = 12
            c.pointer(x1, y1, 0); time.sleep(0.05); c.pointer(x1, y1, 1)
            for k in range(1, steps + 1):
                c.pointer(x1 + (x2 - x1) * k // steps, y1 + (y2 - y1) * k // steps, 1); time.sleep(0.03)
            c.pointer(x2, y2, 0); i += 5
        elif cmd == "key":
            name = a[i + 1]
            sym = KEYSYMS.get(name) or ord(name)
            c.key(sym, True); time.sleep(0.05); c.key(sym, False); i += 2
        elif cmd == "type":
            for ch in a[i + 1]:
                sym = ord(ch)
                c.key(sym, True); time.sleep(0.03); c.key(sym, False); time.sleep(0.03)
            i += 2
        elif cmd == "wheel":
            x, y, direction, n = int(a[i + 1]), int(a[i + 2]), a[i + 3], int(a[i + 4])
            bit = 8 if direction == "up" else 16
            c.pointer(x, y, 0)
            for _ in range(n):
                c.pointer(x, y, bit); time.sleep(0.05); c.pointer(x, y, 0); time.sleep(0.15)
            i += 5
        elif cmd == "wait":
            time.sleep(float(a[i + 1])); i += 2
        else:
            raise SystemExit("bad command " + cmd)
        time.sleep(0.15)


main()
