#!/usr/bin/env python3
"""fm1t - Mac-side client for the FM-1 Transporter data channel.

Read-only by design: the firmware has no erase or write request.

    fm1t.py status
    fm1t.py info
    fm1t.py dump out.bin [--addr 0] [--len 0x100000] [--compare ref.bin]

The transporter must already have the FM-1 in UBOOT (see the console log).
Requires pyserial.
"""

import argparse
import glob
import sys
import time
import zlib

import serial

FLASH_SIZE = 0x100000


def find_port(explicit=None):
    candidates = [explicit] if explicit else sorted(glob.glob("/dev/cu.usbmodem*"))
    for path in candidates:
        try:
            s = serial.Serial(path, 115200, timeout=0.3)
        except serial.SerialException:
            continue
        s.reset_input_buffer()
        s.write(b"ping\n")
        deadline = time.time() + 1.0
        buf = b""
        while time.time() < deadline:
            buf += s.read(64)
            if b"PONG" in buf:
                s.timeout = 5
                return s
        s.close()
    sys.exit("fm1t: no FM-1 Transporter data port found")


def request(s, line, timeout=30):
    s.reset_input_buffer()
    s.write(line.encode() + b"\n")
    s.timeout = timeout
    reply = s.readline().decode(errors="replace").strip()
    if not reply:
        sys.exit(f"fm1t: no reply to '{line}'")
    return reply


def cmd_status(s, _):
    print(request(s, "status"))


def cmd_info(s, _):
    reply = request(s, "info", timeout=30)
    print(reply)
    if reply.startswith("OK"):
        fields = dict(kv.split("=") for kv in reply.split()[1:])
        ok = fields.get("key") == "980F" and fields.get("id") == "856014"
        print("expected WL82 key 980F and flash 856014:", "yes" if ok else "NO")


def cmd_dump(s, args):
    addr, length = args.addr, args.len
    t0 = time.time()
    reply = request(s, f"read {addr:#x} {length}", timeout=30)
    if not reply.startswith("DATA "):
        sys.exit(f"fm1t: {reply}")
    n = int(reply.split()[1])

    data = bytearray()
    s.timeout = 10
    while len(data) < n:
        chunk = s.read(min(65536, n - len(data)))
        if not chunk:
            sys.exit(f"fm1t: stalled at {len(data):#x} of {n:#x}")
        data += chunk
        print(f"\r{len(data) * 100 // n:3d}%  {len(data):#08x}", end="", flush=True)
    print()

    end = s.readline().decode(errors="replace").strip()
    crc = zlib.crc32(data) & 0xFFFFFFFF
    if end != f"END {crc:08X}":
        sys.exit(f"fm1t: transfer check failed ({end}, local {crc:08X}); not saving")

    with open(args.out, "wb") as f:
        f.write(data)
    dt = time.time() - t0
    print(f"saved {args.out}: {n} bytes from {addr:#x}, crc32 {crc:08X}, "
          f"{dt:.1f} s ({n / dt / 1024:.0f} KiB/s)")

    if args.compare:
        compare(args.compare, addr, bytes(data))


APP_END = 0x93000   # [0, APP_END) is the V15 package region; must match exactly
SECTOR = 0x1000


def compare(ref_path, addr, data):
    """Per-4 KiB comparison. Past APP_END, V15 rewrites device data (VM/BTIF/USR),
    so differences there are reported, not treated as failures."""
    ref = open(ref_path, "rb").read()
    bad_app, changed_data = [], []
    for off in range(0, len(data), SECTOR):
        a = addr + off
        mine = data[off:off + SECTOR]
        theirs = ref[a:a + len(mine)]
        if len(theirs) < len(mine):
            break
        if mine != theirs:
            (bad_app if a < APP_END else changed_data).append(a)
    covered = min(len(data), max(0, len(ref) - addr))
    print(f"compared {covered:#x} bytes against {ref_path}")
    if bad_app:
        print(f"MISMATCH in [0, {APP_END:#x}): sectors " + " ".join(f"{a:#07x}" for a in bad_app))
    elif addr < APP_END:
        print(f"[{addr:#x}, {min(addr + covered, APP_END):#x}) identical")
    if changed_data:
        print(f"device-data sectors differing (normal after V15 runs): "
              + " ".join(f"{a:#07x}" for a in changed_data))
    elif addr + covered > APP_END:
        print(f"[{max(addr, APP_END):#x}, {addr + covered:#x}) identical")
    if bad_app:
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="data port (default: probe /dev/cu.usbmodem*)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    sub.add_parser("info")
    d = sub.add_parser("dump")
    d.add_argument("out")
    d.add_argument("--addr", type=lambda x: int(x, 0), default=0)
    d.add_argument("--len", type=lambda x: int(x, 0), default=FLASH_SIZE)
    d.add_argument("--compare", help="reference image to compare against")
    args = ap.parse_args()

    s = find_port(args.port)
    {"status": cmd_status, "info": cmd_info, "dump": cmd_dump}[args.cmd](s, args)


if __name__ == "__main__":
    main()
