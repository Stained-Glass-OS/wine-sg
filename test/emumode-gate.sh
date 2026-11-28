#!/bin/sh
# A program's display mode change in a Stained Glass session (patches/sg/9555):
# the shell's desktop spans the host's monitors (HostMonitors=Y, 7749), so no
# host mode changes; the program's mode is emulated (Wine scales its picture to
# the monitor). Before 9555 the change was answered "successful" and undone at
# once -- the next look at the devices wrote the monitor's own mode back -- and
# the game found its 640x480 was the monitor's 1920x1080.
#
# Run IN a session (the QA VM), as its user, with the session's environment
# (its display and its Wine prefix): it starts test/emumode-probe.c in the session's own
# Wine. SKIPs anywhere else (not asked for, no Xwayland display, HostMonitors not
# set): it is no host gate.
#   SG_SESSION_GATE=1 WINE=/opt/wine-sg/bin/wine test/emumode-gate.sh [PROBE.exe]   (in the VM's session)
# Mutant (win32u, -DSG_MUTANT_EMULATED_MODE_DROPPED): the mode is undone again.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
# It changes the display mode of the session it runs in: never a person's own desktop, so it runs only when
# asked to (SG_SESSION_GATE=1, in a QA VM's session) -- an agent's shell on a host inherits the person's own display.
[ "${SG_SESSION_GATE:-}" = 1 ] || { echo "SKIP: a session gate (SG_SESSION_GATE=1, in a QA VM's session only)"; exit 77; }
[ -n "${DISPLAY:-}" ] || { echo "SKIP: not in a session (no display)"; exit 77; }
xdpyinfo 2>/dev/null | grep -q XWAYLAND || { echo "SKIP: not a session's Xwayland display"; exit 77; }
"$WINE" reg query 'HKCU\Software\Wine\Explorer' /v HostMonitors 2>/dev/null | tr -d '\r' | grep -q 'REG_SZ *Y' \
    || { echo "SKIP: the desktop does not span the host's monitors"; exit 77; }
PROBE=${1:-}
T=$(mktemp -d /var/tmp/sg-emumode.XXXXXX)
trap 'rm -rf "$T"' EXIT INT TERM
if [ -z "$PROBE" ]; then
    MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
    command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing (or pass a built probe)"; exit 77; }
    "$MINGW" -O1 -mwindows -o "$T/emumode-probe.exe" "$HERE/emumode-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
    PROBE="$T/emumode-probe.exe"
fi
desk() { xwininfo -root -tree 2>/dev/null | grep '"shell - Wine Desktop"' | head -1 | sed 's/.*)  *\([0-9]*x[0-9]*\)+.*/\1/'; }
D0=$(desk)
OUT="$T/o.txt"
timeout -s KILL 60 "$WINE" "$PROBE" "Z:$(echo "$OUT" | tr / '\\')" 640 480 3000 &
PP=$!
i=0; while [ $i -lt 300 ] && ! grep -q '^HELD' "$OUT" 2>/dev/null; do python3 -c 'import time; time.sleep(0.1)'; i=$((i + 1)); done
D1=$(desk)
STATE=$(xprop -name "shell - Wine Desktop" _NET_WM_STATE 2>/dev/null)
wait $PP
tr -d '\r' < "$OUT" > "$T/r.txt" 2>/dev/null
cat "$T/r.txt"
phys=$(sed -n 's/^START mode \([0-9]*x[0-9]*\)@.*/\1/p' "$T/r.txt")
grep -q '^MODES [0-9]* listed 1' "$T/r.txt" && pass "640x480 is offered" || fail "640x480 is not among the modes"
grep -q '^CDS 640x480@60 0$' "$T/r.txt" && pass "the change to 640x480 at 60 Hz is taken" || fail "the change: $(grep '^CDS' "$T/r.txt")"
grep -q '^AFTER mode 640x480@[0-9]* screen 640x480 monitor 640x480' "$T/r.txt" && pass "the program finds 640x480 at once" \
    || fail "at once: $(grep '^AFTER' "$T/r.txt")"
grep -q '^LATER mode 640x480@[0-9]* screen 640x480 monitor 640x480' "$T/r.txt" && pass "and after the change went round (3 s)" \
    || fail "later: $(grep '^LATER' "$T/r.txt")"
[ -n "$D0" ] && [ "$D1" = "$D0" ] && pass "the shell's desktop stays the monitor's size ($D1)" || fail "the desktop: $D0 -> $D1"
case "$STATE" in *FULLSCREEN*) pass "and stays full screen";; *) fail "the desktop is not full screen: $STATE";; esac
grep -q "^RESTORED mode $phys@" "$T/r.txt" && pass "the monitor's own mode ($phys) comes back" || fail "restored: $(grep '^RESTORED' "$T/r.txt")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
