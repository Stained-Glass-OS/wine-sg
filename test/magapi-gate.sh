#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Magnification API on the session's compositor (7845): the compositor draws the magnified
# screen (sg-compositor's magnifier.c), so Magnifier magnifies X and Wayland programs too -- David
# 2026-11-17, "Magnifier does not seem to work on Linux apps". magnification.dll holds one MAGNIFY
# connection on the compositor's control socket (SG_LOCK_CONTROL) and says what to show, a line each.
# A stand-in compositor (a Unix socket) records what it is told:
#   - MagSetFullscreenTransform(2.5, 100, 50) -> "full 250 100 50", read back by MagGetFullscreenTransform
#   - an inverting colour matrix -> "invert 1"; SgMagCommand passes Magnifier's views ("lens 200 300 200")
#   - what the compositor refuses (ERR) fails, as does a level below 1
#   - MagUninitialize ends the connection (so magnification ends), 64- and 32-bit callers alike
#   - with no compositor the calls fail (Magnifier then draws its views itself) and nothing hangs
#   - the compositor is given physical pixels: at 200% (LogPixels 192) a DPI-unaware caller's offsets
#     are doubled, a per-monitor aware caller's kept; less the virtual screen's top left (7846)
#   - a magnifier control ("Magnifier" class, MagSetWindowSource/Transform, 7847): the compositor is
#     told its source and where it is (window ID 10 20 150 100 -> 100 100 300 200 at 200%), what of it
#     is not covered when a window comes over half of it, its inverted colours, and that it is gone
#   WINE=/opt/wine-sg/bin/wine test/magapi-gate.sh   (mutants SG_MUTANT_MAG_KEEP_CONNECTION, unixlib.c;
#   SG_MUTANT_MAG_LOGICAL_COORDS, magnification.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-magapi.XXXXXX)
SP=
cleanup() { [ -n "${XP:-}" ] && kill "$XP" 2>/dev/null; [ -n "$SP" ] && kill "$SP" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T" 2>/dev/null; }
trap cleanup EXIT INT TERM
ARCHES=
for a in x86_64 i686; do
    command -v "$a-w64-mingw32-gcc" >/dev/null || continue
    TMPDIR=/var/tmp "$a-w64-mingw32-gcc" -O1 -o "$T/probe-$a.exe" "$HERE/magapi-probe.c" || { fail "the $a probe did not build"; continue; }
    TMPDIR=/var/tmp "$a-w64-mingw32-gcc" -O1 -o "$T/ctl-$a.exe" "$HERE/magctl-probe.c" || { fail "the $a control probe did not build"; continue; }
    ARCHES="$ARCHES $a"
done
[ -n "$ARCHES" ] || { echo "SKIP: no mingw"; exit 77; }
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/fake.py" <<'PY'
import os, socket, sys, threading, time
path, log = sys.argv[1], open(sys.argv[2], "a", buffering=1)
s = socket.socket(socket.AF_UNIX); s.bind(path); s.listen(4)
def serve(c):
    f = c.makefile("rb")
    first = f.readline().decode().strip()
    log.write("OPEN %s\n" % first)
    if first != "MAGNIFY":
        c.close(); return
    c.sendall(b"OK magnify\n")
    for line in f:
        line = line.decode().strip()
        log.write("LINE %s\n" % line)
        c.sendall(b"ERR\n" if line == "nonsense" else b"OK\n")
    log.write("CLOSED %.3f\n" % time.time())
while True:
    c, _ = s.accept()
    threading.Thread(target=serve, args=(c,), daemon=True).start()
PY
python3 "$T/fake.py" "$T/ctl.sock" "$T/comp.log" &
SP=$!
for i in 1 2 3 4 5 6 7 8 9 10; do [ -S "$T/ctl.sock" ] && break; sleep 0.2; done

for a in $ARCHES; do
    : > "$T/comp.log"
    out=$(SG_LOCK_CONTROL="$T/ctl.sock" timeout -s KILL 60 "$WINE" "$T/probe-$a.exe" 2>/dev/null | tr -d '\r')
    ended=$(date +%s.%N)
    sleep 0.5
    printf '%s\n' "$out" | sed "s/^/      $a: /"; sed "s/^/      $a compositor: /" "$T/comp.log"
    want="init 1
set 1
get 250 100 50
effect 1
lens 1
bad 0
small 0
uninit 1
done"
    [ "$out" = "$want" ] && pass "$a: the calls answer as they should (set, read back, invert, a view; refused and too small fail)" \
        || fail "$a: the probe said something else"
    grep -qx "LINE full 250 100 50" "$T/comp.log" && grep -qx "LINE invert 1" "$T/comp.log" && grep -qx "LINE lens 200 300 200" "$T/comp.log" \
        && pass "$a: the compositor was told: full 250 100 50, invert 1, lens 200 300 200" || fail "$a: the compositor was not told what to show"
    [ "$(grep -c '^OPEN MAGNIFY' "$T/comp.log")" = 1 ] && pass "$a: one connection for all of it" || fail "$a: $(grep -c '^OPEN' "$T/comp.log") connections"
    # the connection ends at MagUninitialize, before the program does (it waits 1.5 s after it)
    awk -v e="$ended" '/^CLOSED/ { c = ($2 < e - 1.0) } END { exit !c }' "$T/comp.log" && pass "$a: MagUninitialize ends the connection (and with it, magnification)" \
        || fail "$a: the connection outlived MagUninitialize"
done

# no compositor: the calls fail at once, nothing waits
a=${ARCHES# }; a=${a%% *}
start=$(date +%s)
out=$(SG_LOCK_CONTROL="$T/none.sock" timeout -s KILL 60 "$WINE" "$T/probe-$a.exe" 2>/dev/null | tr -d '\r')
took=$(( $(date +%s) - start ))
printf '%s\n' "$out" | grep -qx "set 0" && printf '%s\n' "$out" | grep -qx "lens 0" && printf '%s\n' "$out" | grep -qx done \
    && pass "with no compositor the calls fail (Magnifier draws its views itself), in ${took} s" \
    || fail "with no compositor: $(printf '%s' "$out" | tr '\n' ' ')"
# the magnifier control (windows: a display of its own)
XP=
if command -v Xvfb >/dev/null; then
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
[ -s "$T/display" ] || { echo "SKIP: no Xvfb for the magnifier control"; ARCHES_CTL=; }
CDISPLAY=":$(cat "$T/display" 2>/dev/null)"
# the desktop the earlier runs made has no display: a new one with it
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
for a in ${ARCHES_CTL-$ARCHES}; do
    : > "$T/comp.log"
    out=$(DISPLAY="$CDISPLAY" SG_LOCK_CONTROL="$T/ctl.sock" timeout -s KILL 60 "$WINE" "$T/ctl-$a.exe" 2>/dev/null | tr -d '\r')
    sleep 0.5
    printf '%s\n' "$out" | sed "s/^/      $a: /"; sed "s/^/      $a compositor: /" "$T/comp.log"
    id=$(printf '%s\n' "$out" | awk '/^id / {print $2}')
    printf '%s\n' "$out" | grep -qx "getsource 10 20 160 120" && printf '%s\n' "$out" | grep -qx done \
        && pass "$a: the control takes its source and reads it back" || fail "$a: the control probe: $(printf '%s' "$out" | tr '\n' ' ')"
    grep -qx "LINE window $id 10 20 150.000 100.000 100 100 300 200 0 100 100 300 200" "$T/comp.log" \
        && pass "$a: the compositor shows 10,20 150x100 at 200% in the control at 100,100 300x200" \
        || fail "$a: the control's view was not told as it should be"
    grep -qx "LINE window $id 10 20 150.000 100.000 100 100 300 200 0 100 100 150 200" "$T/comp.log" \
        && pass "$a: a window over its right half: only its left half is shown" || fail "$a: the covered part was not left out"
    grep -q "^LINE window $id 10 20 150.000 100.000 100 100 300 200 1 " "$T/comp.log" \
        && pass "$a: MagSetColorEffect (inverting): the control's view is inverted" || fail "$a: the inverted control was not told"
    grep -qx "LINE window $id off" "$T/comp.log" && pass "$a: the control destroyed: its view goes" || fail "$a: its view was not ended"
done
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null

# 200%: a DPI-unaware program's screen point is the physical one halved
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 192 /f >/dev/null 2>&1
"$WINESERVER" -w
for mode in unaware aware; do
    : > "$T/comp.log"
    out=$(SG_LOCK_CONTROL="$T/ctl.sock" timeout -s KILL 60 "$WINE" "$T/probe-$a.exe" $mode 2>/dev/null | tr -d '\r')
    sleep 0.5
    want="LINE full 250 100 50"; [ $mode = unaware ] && want="LINE full 250 200 100"
    got=$(grep '^LINE full' "$T/comp.log")
    [ "$got" = "$want" ] && printf '%s\n' "$out" | grep -qx "get 250 100 50" \
        && pass "at 200%, a DPI-$mode caller's offset 100,50 reaches the compositor as ${want#LINE full 250 } (and reads back as given)" \
        || fail "at 200%, DPI-$mode: the compositor was told '${got#LINE }' (wanted '${want#LINE }'); $(printf '%s' "$out" | grep '^get')"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
