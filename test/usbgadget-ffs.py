#!/usr/bin/env python3
# usbgadget-ffs.py FFS_MOUNT: the WinUSB test gadget's function (FunctionFS), for
# test/usbgadget-setup.sh (patches/sg/6519-6520). One vendor-specific interface with a bulk IN
# (0x81) and a bulk OUT (0x02) endpoint, Microsoft OS 1.0 descriptors naming the compatible ID
# "WINUSB" and a DeviceInterfaceGUID. What it does:
#   bulk OUT -> bulk IN: echoes what it reads; data starting "PATTERN" is answered with 1024
#                        bytes, byte i = i % 251
#   vendor IN  request 0x01: "SGWINUSB"
#   vendor OUT request 0x02: keeps the data; vendor IN request 0x03 gives it back
# Written from the Linux kernel's FunctionFS documentation and the public descriptor formats.
import os, struct, sys, threading

FFS = sys.argv[1]
READY = sys.argv[2] if len(sys.argv) > 2 else None
GUID = "{5347A11C-0B5B-4F3A-9C2E-6D1E5B0F57A1}"

def endpoint(address, size):
    return struct.pack('<BBBBHB', 7, 5, address, 2, size, 0)

def descriptors():
    interface = struct.pack('<BBBBBBBBB', 9, 4, 0, 0, 2, 0xff, 0, 0, 1)
    fs = interface + endpoint(0x81, 64) + endpoint(0x02, 64)
    hs = interface + endpoint(0x81, 512) + endpoint(0x02, 512)
    # OS descriptor header: interface, dwLength (with the header), bcdVersion (FunctionFS wants 1),
    # wIndex, bCount/wCount
    compat_entry = struct.pack('<BB8s8s6s', 0, 1, b'WINUSB', b'', b'')
    compat = struct.pack('<BIHHBB', 0, 11 + len(compat_entry), 1, 4, 1, 0) + compat_entry
    name = 'DeviceInterfaceGUID\0'.encode('utf-16-le')
    data = (GUID + '\0').encode('utf-16-le')
    prop = struct.pack('<IIH', 14 + len(name) + len(data), 1, len(name)) + name + struct.pack('<I', len(data)) + data
    ext = struct.pack('<BIHHH', 0, 11 + len(prop), 1, 5, 1) + prop
    body = struct.pack('<III', 3, 3, 2) + fs + hs + compat + ext
    flags = 1 | 2 | 8      # full speed, high speed, MS OS descriptors
    return struct.pack('<III', 3, 12 + len(body), flags) + body

def strings():
    text = b'WinUSB test interface\0'
    body = struct.pack('<H', 0x0409) + text
    return struct.pack('<IIII', 2, 16 + len(body), 1, 1) + body

stored = b''
enabled = threading.Event()

def control(ep0):
    global stored
    while True:
        event = os.read(ep0, 12)
        if len(event) < 12: continue
        kind = event[8]
        if kind == 2:                       # ENABLE
            enabled.set()
        elif kind == 4:                     # SETUP
            request_type, request, value, index, length = struct.unpack('<BBHHH', event[:8])
            vendor = (request_type & 0x60) == 0x40
            if request_type & 0x80:
                if vendor and request == 0x01: os.write(ep0, b'SGWINUSB'[:length])
                elif vendor and request == 0x03: os.write(ep0, stored[:length])
                else: os.read(ep0, 0)       # stall
            else:
                if vendor and request == 0x02: stored = os.read(ep0, length) if length else b''
                else: os.write(ep0, b'')    # stall

def bulk():
    enabled.wait()
    ep_in = os.open(os.path.join(FFS, 'ep1'), os.O_RDWR)
    ep_out = os.open(os.path.join(FFS, 'ep2'), os.O_RDWR)
    pattern = bytes(i % 251 for i in range(1024))
    while True:
        try:
            data = os.read(ep_out, 65536)
        except OSError:
            continue
        try:
            os.write(ep_in, pattern if data.startswith(b'PATTERN') else data)
        except OSError:
            pass

ep0 = os.open(os.path.join(FFS, 'ep0'), os.O_RDWR)
os.write(ep0, descriptors())
os.write(ep0, strings())
if READY:
    open(READY, 'w').close()
threading.Thread(target=bulk, daemon=True).start()
control(ep0)
