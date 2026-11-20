#!/usr/bin/env python3
"""Check a WAYLAND_DEBUG=client log from the Marathon host.

Passes only when the client created an xdg_toplevel through get_toplevel,
called set_app_id on that same toplevel with the expected id, acknowledged a
configure, and committed a wl_surface that had a wl_buffer attached.

This is a generic Wayland protocol check against whatever compositor the log
came from. It says nothing about MarathonOS.
"""
import re
import sys

APP_ID = "org.rnqnx.marathon.playground"

# WAYLAND_DEBUG lines look like:
#   [ 123.456]  -> xdg_surface@14.get_toplevel(new id xdg_toplevel@15)
TOPLEVEL = re.compile(r"-> xdg_surface@(\d+)\.get_toplevel\(new id xdg_toplevel@(\d+)\)")
SET_APP_ID = re.compile(r'-> xdg_toplevel@(\d+)\.set_app_id\("([^"]*)"\)')
ACK = re.compile(r"-> xdg_surface@(\d+)\.ack_configure\(\d+\)")
ATTACH = re.compile(r"-> wl_surface@(\d+)\.attach\(wl_buffer@\d+, ")
COMMIT = re.compile(r"-> wl_surface@(\d+)\.commit\(\)")
GET_XDG_SURFACE = re.compile(r"-> xdg_wm_base@\d+\.get_xdg_surface\(new id xdg_surface@(\d+), wl_surface@(\d+)\)")


def check(text: str) -> list[str]:
    problems: list[str] = []
    xdg_to_wl = {m.group(1): m.group(2) for m in GET_XDG_SURFACE.finditer(text)}
    toplevels = {m.group(2): m.group(1) for m in TOPLEVEL.finditer(text)}
    if not toplevels:
        return ["no xdg_surface.get_toplevel request in the client log"]

    ids = {m.group(1): m.group(2) for m in SET_APP_ID.finditer(text)}
    matching = [t for t, app_id in ids.items() if t in toplevels and app_id == APP_ID]
    if not matching:
        seen = sorted(set(ids.values()))
        return [f"no set_app_id({APP_ID!r}) on a created xdg_toplevel; app ids seen: {seen}"]

    xdg_surface = toplevels[matching[0]]
    wl_surface = xdg_to_wl.get(xdg_surface)
    if wl_surface is None:
        problems.append(f"xdg_surface@{xdg_surface} was not created from a wl_surface")
    if not any(m.group(1) == xdg_surface for m in ACK.finditer(text)):
        problems.append(f"xdg_surface@{xdg_surface} never acknowledged a configure")
    if wl_surface is not None:
        if not any(m.group(1) == wl_surface for m in ATTACH.finditer(text)):
            problems.append(f"wl_surface@{wl_surface} never had a wl_buffer attached")
        if not any(m.group(1) == wl_surface for m in COMMIT.finditer(text)):
            problems.append(f"wl_surface@{wl_surface} was never committed")
    return problems


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: check-wayland-client-log.py <wayland-client.log>", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8", errors="replace") as handle:
        problems = check(handle.read())
    if problems:
        for problem in problems:
            print(f"FAIL {problem}", file=sys.stderr)
        return 1
    print(f"PASS xdg_toplevel created with app_id {APP_ID}, configured, buffer attached and committed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
