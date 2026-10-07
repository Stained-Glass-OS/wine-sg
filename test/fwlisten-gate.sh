#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The firewall learns which Windows program listens (patches/sg/1500). A
# Windows program's sockets are wineserver's, so Stained Glass Firewall
# (sg-session's sg-firewall) cannot tell from the system whose they are;
# Wine tells it: on a listen (TCP) or a bind (UDP) on an address other
# computers can reach, a datagram to its notify socket with the program's
# Windows path and the socket itself (SCM_RIGHTS), the kernel adding the
# sender's uid. Here a stand-in for the firewall receives them: the probe's
# TCP listeners (IPv4 and IPv6) and its UDP socket are told, with the right
# ports and C:\fwprobe.exe, and so is a UDP socket it never bound that
# sends to a multicast group (device discovery: SSDP); its loopback ones, a
# TCP socket that never listens and a unicast sender are not.
#
#   WINE=/opt/wine-sg/bin/wine test/fwlisten-gate.sh   (mutant SG_MUTANT_FW_LISTEN_NOTIFY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: no python3"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fwlisten.XXXXXX)
RPID=""
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export SG_FIREWALL_NOTIFY="$T/notify"
trap '[ -n "$RPID" ] && kill "$RPID" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/fwprobe.exe" "$HERE/fwlisten-probe.c" -lws2_32 || { fail "probe did not build"; exit 1; }

# the firewall's stand-in: what each notice says, and what its socket is
cat > "$T/receiver.py" <<'PY'
import array, os, socket, struct, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_PASSCRED, 1)
s.bind(sys.argv[1])
s.settimeout(1)
out = open(sys.argv[2], "a")
end = time.time() + 240
while time.time() < end:
    try:
        msg, anc, _f, _a = s.recvmsg(4096, socket.CMSG_SPACE(16) + socket.CMSG_SPACE(12))
    except socket.timeout:
        continue
    fds, uid = [], -1
    for level, typ, data in anc:
        if typ == socket.SCM_RIGHTS:
            a = array.array("i"); a.frombytes(data[:len(data) - len(data) % 4]); fds += list(a)
        elif typ == socket.SCM_CREDENTIALS:
            uid = struct.unpack("3i", data[:12])[1]
    proto, port = "none", 0
    if fds:
        k = socket.socket(fileno=os.dup(fds[0]))
        proto = {socket.SOCK_STREAM: "tcp", socket.SOCK_DGRAM: "udp"}.get(k.type, "?")
        port = k.getsockname()[1]
        k.close()
        for fd in fds:
            os.close(fd)
    lines = msg.decode("utf-8", "replace").split("\n")
    out.write("%s|%s|%s|%d|%d\n" % (lines[0], lines[1] if len(lines) > 1 else "", proto, port, uid))
    out.flush()
PY
python3 "$T/receiver.py" "$T/notify" "$T/got" &
RPID=$!
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/fwprobe.exe" "$WINEPREFIX/drive_c/fwprobe.exe"
TP=$((20000 + $$ % 20000)); UP=$((TP + 10))
: > "$T/got"
timeout 120 "$WINE" 'C:\fwprobe.exe' "$TP" "$UP" 2>/dev/null | tr -d '\r' > "$T/o"
sleep 1
grep -q '^DONE' "$T/o" || fail "the probe did not finish: $(tr '\n' ' ' < "$T/o")"
got() { grep -c "^SGFW1|C:\\\\fwprobe.exe|$1|$2|$(id -u)\$" "$T/got"; }
[ "$(got tcp "$TP")" = 1 ] && pass "a TCP listener on every address: the firewall is told (C:\\fwprobe.exe, its port, our uid, the socket itself)" \
    || fail "TCP $TP: $(tr '\n' ' ' < "$T/got")"
[ "$(got tcp $((TP + 2)))" = 1 ] && pass "an IPv6 listener too" || fail "IPv6 TCP $((TP + 2)): $(tr '\n' ' ' < "$T/got")"
[ "$(got udp "$UP")" = 1 ] && pass "a bound UDP socket too" || fail "UDP $UP: $(tr '\n' ' ' < "$T/got")"
[ "$(got tcp $((TP + 1)))" = 0 ] && [ "$(got udp $((UP + 1)))" = 0 ] && pass "nothing about the loopback's" || fail "told about a loopback socket"
[ "$(got tcp $((TP + 3)))" = 0 ] && pass "nothing about a TCP socket that does not listen" || fail "told about a bound, not listening, TCP socket"
MP=$(sed -n 's/^MULTICAST //p' "$T/o"); UCP=$(sed -n 's/^UNICAST //p' "$T/o")
[ -n "$MP" ] && [ "$(got udp "$MP")" = 1 ] && pass "a UDP socket never bound that looks for devices (multicast): told once, with the port it was given" \
    || fail "multicast $MP: $(tr '\n' ' ' < "$T/got")"
[ -n "$UCP" ] && [ "$(got udp "$UCP")" = 0 ] && pass "nothing about one that sends to an ordinary address" || fail "told about a unicast sender"
[ "$(wc -l < "$T/got")" = 4 ] && pass "four notices, no more" || fail "$(wc -l < "$T/got") notices: $(tr '\n' ' ' < "$T/got")"
# no firewall running: a program still listens (nothing waits for it)
kill "$RPID" 2>/dev/null; RPID=""
rm -f "$T/notify"
timeout 60 "$WINE" 'C:\fwprobe.exe' "$((TP + 100))" "$((UP + 100))" 2>/dev/null | tr -d '\r' > "$T/o2"
grep -q "^LISTEN 0.0.0.0 $((TP + 100))" "$T/o2" && grep -q '^DONE' "$T/o2" && pass "without the firewall a program listens as before" \
    || fail "without the firewall: $(tr '\n' ' ' < "$T/o2")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
