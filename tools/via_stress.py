#!/usr/bin/env python3
"""Send VIA commands back-to-back (like the VIA web app does) and check that
each response matches its command. Reports lagging/duplicated responses."""
import os, select, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from via_probe import find_raw_hid, drain

def run(fd, n, pipelined):
    bad = dup = 0
    for i in range(n):
        off = (i * 28) % 0x900
        req = bytes([0x0E, off >> 8, off & 0xFF, 28])
        os.write(fd, b"\0" + req.ljust(32, b"\0"))
        if pipelined:
            continue
        r, _, _ = select.select([fd], [], [], 1.0)
        if not r:
            print("timeout at", i); bad += 1; continue
        rep = os.read(fd, 64)
        if rep[:4] != req:
            bad += 1
        extra = drain(fd, 0.005)
        dup += len(extra)
    if pipelined:
        got = drain(fd, 1.0)
        print(f"pipelined: sent {n}, got {len(got)} reports")
    else:
        print(f"sequential: {n} cmds, mismatched={bad}, extra reports={dup}")

fd = os.open(find_raw_hid(), os.O_RDWR)
drain(fd, 0.2)
run(fd, 200, False)
run(fd, 50, True)
