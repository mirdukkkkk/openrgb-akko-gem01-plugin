#!/usr/bin/env python3
"""Probe the VIA raw-HID interface of an AKKO Gem 01 directly via /dev/hidraw*.

Sends a few VIA commands and prints *every* report received for each one, so
duplicated / delayed responses become visible.
"""
import glob, os, select, sys, time

VID, PID = 0x342D, 0xE4E3

def find_raw_hid():
    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        ue = open(f"{node}/device/uevent").read()
        if f"{VID:08X}:{PID:08X}" not in ue:
            continue
        desc = open(f"{node}/device/report_descriptor", "rb").read()
        if desc.startswith(bytes([0x06, 0x60, 0xFF, 0x09, 0x61])):
            return "/dev/" + os.path.basename(node)
    return None

def drain(fd, timeout):
    out, end = [], time.monotonic() + timeout
    while (left := end - time.monotonic()) > 0:
        r, _, _ = select.select([fd], [], [], left)
        if not r:
            break
        out.append(os.read(fd, 64))
    return out

def cmd(fd, data, timeout=0.3):
    os.write(fd, bytes([0]) + bytes(data).ljust(32, b"\0"))
    t = time.monotonic()
    reps = drain(fd, timeout)
    print(f"-> {' '.join(f'{b:02x}' for b in data)}")
    for r in reps:
        print(f"   <- {' '.join(f'{b:02x}' for b in r[:8])} ...")
    if not reps:
        print("   <- (no response)")
    return reps

def main():
    dev = sys.argv[1] if len(sys.argv) > 1 else find_raw_hid()
    print("raw HID node:", dev)
    fd = os.open(dev, os.O_RDWR)
    stale = drain(fd, 0.2)
    print(f"stale reports before start: {len(stale)}")
    cmd(fd, [0x01])                 # GET_PROTOCOL_VERSION
    cmd(fd, [0x02, 0x01])           # GET_KEYBOARD_VALUE uptime
    for vid in (1, 2, 3, 4):        # rgb_matrix channel 3: brightness/effect/speed/color
        cmd(fd, [0x08, 0x03, vid])
    cmd(fd, [0x0D])                 # MACRO_GET_BUFFER_SIZE
    cmd(fd, [0x0E, 0x00, 0x00, 28]) # MACRO_GET_BUFFER
    cmd(fd, [0x0E, 0x00, 28, 28])

if __name__ == "__main__":
    main()
