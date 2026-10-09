#!/usr/bin/env python3
"""A virtual Xbox-style gamepad (045e:028e, version 0x0110) on /dev/uinput, for test/wgi-device-gate.sh.
Prints "created" and then runs until killed."""
import os, struct, fcntl, time
fd=os.open('/dev/uinput',os.O_WRONLY|os.O_NONBLOCK)
io=lambda r,v: fcntl.ioctl(fd,r,v)
io(0x40045564,1); io(0x40045564,3)
for k in range(0x130,0x13f): io(0x40045565,k)
axes=(0,1,2,3,4,5,0x10,0x11)
for a in axes: io(0x40045567,a)
io(0x405c5503,struct.pack('HHHH80sI',0x03,0x045e,0x028e,0x0110,b'SG Test Pad',0))
for a in axes:
    lo,hi=(-32768,32767) if a<0x10 else (-1,1)
    io(0x401c5504,struct.pack('H2x6i',a,0,lo,hi,*((16,128,0) if a<0x10 else (0,0,0))))
io(0x5501,0)
print('created',flush=True)
while True: time.sleep(1)
