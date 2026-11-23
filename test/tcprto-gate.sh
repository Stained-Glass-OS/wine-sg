#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ws2_32/ntdll: SIO_TCP_INITIAL_RTO (patches/sg/8900; test/tcprto-probe.c). libuv (every Node/Electron
# program) and the JDK set it before a loopback connect; Wine answered "unimplemented ioctl". The gate
# also reads the probe's sockets from the Linux side (pidfd_getfd) and checks TCP_SYNCNT: 3, 1 (no
# retransmissions: the fewest Linux makes) and 9. That part is skipped where ptrace is not allowed.
#   WINE=/opt/wine-sg/bin/wine test/tcprto-gate.sh
# Mutant: SG_MUTANT_NO_INITIAL_RTO (dlls/ws2_32/socket.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-tcprto.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/tcprto-probe.exe" "$HERE/tcprto-probe.c" -lws2_32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/tcprto-probe32.exe" "$HERE/tcprto-probe.c" -lws2_32 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# the TCP_SYNCNT of every TCP socket the prefix's wineserver holds (Wine's sockets live in the server),
# read through pidfd_getfd; the server is the one whose working directory is the prefix's server dir
cat > "$T/syncnt.py" <<'PY'
import ctypes, os, socket, sys
st = os.stat(sys.argv[1]); want = 'server-%x-%x' % (st.st_dev, st.st_ino)
libc = ctypes.CDLL(None, use_errno=True); vals = []; found = False
for pid in os.listdir('/proc'):
    if not pid.isdigit(): continue
    try:
        if not os.readlink('/proc/%s/cwd' % pid).endswith('/' + want): continue
        if 'wineserver' not in open('/proc/%s/cmdline' % pid, 'rb').read().decode('latin-1'): continue
        fds = os.listdir('/proc/%s/fd' % pid)
    except OSError: continue
    found = True; pfd = os.pidfd_open(int(pid))
    for fd in fds:
        try:
            if not os.readlink('/proc/%s/fd/%s' % (pid, fd)).startswith('socket:'): continue
        except OSError: continue
        n = libc.syscall(438, pfd, int(fd), 0)   # pidfd_getfd
        if n < 0: print('NOPTRACE'); sys.exit(0)
        s = socket.socket(fileno=n)
        try:
            if s.type == socket.SOCK_STREAM and s.family in (socket.AF_INET, socket.AF_INET6):
                vals.append(s.getsockopt(socket.IPPROTO_TCP, socket.TCP_SYNCNT))
        finally: s.detach(); os.close(n)
print(' '.join(str(v) for v in sorted(vals)) if found else 'NOSERVER')
PY
rc=0
for p in tcprto-probe tcprto-probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    rm -f "$T/go"
    timeout -s KILL 90 "$WINE" "$T/$p.exe" 'Z:'"$(echo "$T/go" | tr / '\\')" 2>/dev/null </dev/null > "$T/$p.raw" &
    i=0; while [ $i -lt 300 ] && ! grep -q READY "$T/$p.raw" 2>/dev/null; do sleep 0.2 2>/dev/null || python3 -c 'import time; time.sleep(0.2)'; i=$((i+1)); done
    vals=$(python3 "$T/syncnt.py" "$WINEPREFIX" 2>&1)
    if [ "$vals" != NOSERVER ]; then
        case "$vals" in
        NOPTRACE*) echo "SKIP  TCP_SYNCNT not readable here (ptrace)";;
        *) for want in 1 3 9; do
               if echo " $vals " | grep -q " $want "; then echo "PASS  a socket's TCP_SYNCNT is $want"
               else echo "FAIL  no socket has TCP_SYNCNT $want (got: $vals)"; rc=1; fi
           done;;
        esac
    else echo "FAIL  the prefix's wineserver not found"; rc=1; fi
    : > "$T/go"; wait
    tr -d '\r' < "$T/$p.raw" > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
