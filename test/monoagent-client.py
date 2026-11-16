#!/usr/bin/env python3
# The debugger side of test/monoagent-gate.sh: speaks Mono's soft debugger protocol (SDB) to the agent on
# its Unix socket PATH (dt_unix, wine-sg 7791) -- handshake, VM VERSION, VM ALL_THREADS, VM RESUME -- and
# prints KEY=VALUE lines. "probe" as a second argument stops after the handshake (another user's try).
# SPDX-License-Identifier: LGPL-2.1-or-later
import socket, struct, sys, time

path = sys.argv[1]
probe = len(sys.argv) > 2 and sys.argv[2] == 'probe'
s = None
for _ in range(1 if probe else 100):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect(path)
        break
    except OSError as e:
        s = None
        if probe:
            print('Error=%s' % e.__class__.__name__)
        time.sleep(0.2)
if not s:
    print('Connect=0')
    sys.exit(0)
print('Connect=1')
s.settimeout(5 if probe else 30)
hs = b''
try:
    s.sendall(b'DWP-Handshake')
    while len(hs) < 13:
        chunk = s.recv(13 - len(hs))
        if not chunk:
            break
        hs += chunk
except OSError:
    pass
print('Handshake=%d' % (hs == b'DWP-Handshake'))
if probe:
    sys.exit(0)

def recv_exact(n):
    data = b''
    while len(data) < n:
        chunk = s.recv(n - len(data))
        if not chunk:
            raise EOFError
        data += chunk
    return data

next_id = 1
def command(cmd_set, cmd, payload=b''):
    global next_id
    pid = next_id
    next_id += 1
    s.sendall(struct.pack('>IIBBB', 11 + len(payload), pid, 0, cmd_set, cmd) + payload)
    while True:
        length, rid, flags = struct.unpack('>IIB', recv_exact(9))
        rest = recv_exact(length - 9)
        if flags & 0x80 and rid == pid:
            return struct.unpack('>H', rest[:2])[0], rest[2:]
        # an event packet (composite events): not ours

err, data = command(1, 1)      # VM VERSION
name_len = struct.unpack('>I', data[:4])[0]
vm = data[4:4 + name_len].decode(errors='replace')
major, minor = struct.unpack('>II', data[4 + name_len:12 + name_len])
print('Version=%d %d %d' % (err, 'mono' in vm.lower(), major >= 2))
err, data = command(1, 2)      # VM ALL_THREADS
count = struct.unpack('>I', data[:4])[0] if not err else 0
print('Threads=%d %d' % (err, count >= 1))
err, data = command(1, 4)      # VM RESUME
print('Resume=%d' % err)
try:
    while s.recv(4096):
        pass
except OSError:
    pass
print('done=1')
