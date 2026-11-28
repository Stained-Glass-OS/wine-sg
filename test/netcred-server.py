#!/usr/bin/python3
# netcred-gate's stand-in for sg-netmountd: a Unix socket at argv[1]; each request line is
# appended to argv[2] and answered "OK".
import os, socket, sys
path, log = sys.argv[1], sys.argv[2]
if os.path.exists(path):
    os.unlink(path)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(path)
s.listen(8)
while True:
    c, _ = s.accept()
    with c:
        f = c.makefile('rwb')
        line = f.readline()
        if line:
            with open(log, 'ab') as o:
                o.write(line)
            f.write(b'OK\n')
            f.flush()
