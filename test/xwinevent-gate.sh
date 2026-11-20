#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An out-of-context WinEvent hook on another process's thread, without a module (wine-sg 8403),
# as Windows allows: wineserver refused it ("module is optional only if hook is in current
# process"), which is right only for hooks that run in the hooked thread. mpv embedded in SMPlayer
# (--wid) follows the size of SMPlayer's video widget through EVENT_OBJECT_LOCATIONCHANGE from
# SMPlayer's thread; without the hook its picture kept the widget's first size (491 of 654 pixels
# wide, the rest black).
# test/xwinevent-probe.c, 64- and 32-bit: a second process makes a window; the probe hooks that
# thread out of context with no module, the other process resizes its child window three times,
# the probe's thread receives the events; an in-context hook into it without a module still fails.
#   WINE=/opt/wine-sg/bin/wine test/xwinevent-gate.sh
# Mutant: -DSG_MUTANT_NO_XPROC_WINEVENT (server/hook.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xwinevent.XXXXXX)
XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill $XP 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/xwinevent-probe.c" -luser32 || { fail "$a probe did not build"; exit 1; }
done
# windows need a display: an X server of the gate's own
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep "^$1=" | head -1 || echo none))"; fi
    }
    check start      "the other process and its window"
    check hook       "SetWinEventHook out of context on another process's thread, no module"
    check incontext  "in context without a module into another process is still refused"
    check delivered  "its EVENT_OBJECT_LOCATIONCHANGEs reach the hooking thread"
    check done        "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
