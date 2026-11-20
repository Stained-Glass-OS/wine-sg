#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Direct2D command list one device context made and another drew into (wine-sg 7759): when that
# other context lets it go, its reference is dropped a while later, so the list outlives the
# program's own last Release. Office's Designer releases such a list after closing it and then
# makes it an effect's input (it works on Windows): the list was freed under it and PowerPoint
# crashed at the first Designer click. A list a context made and drew into itself is freed at its
# last Release as before, and so is one set on another context that was never drawn into; a
# list another context made does not read back as the target, as on Windows (Wine's d2d1 test).
# The device keeps those lists and lets them go when it goes. A context with no target draws nothing (Office then drew into one whose list was refused and
# crashed in d3d11). test/d2dlistref-probe.cpp under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/d2dlistref-gate.sh
# Mutants: SG_MUTANT_D2D_LIST_RELEASE_NOW, SG_MUTANT_D2D_KEEP_LISTS (command_list.c),
# SG_MUTANT_D2D_DRAW_NO_TARGET (device.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DLISTREF_DPY:-392}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dlistref.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T" "${SG_GATE_HOME:-$T}"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -static -o "$T/probe.exe" "$HERE/d2dlistref-probe.cpp" -ld2d1 -ld3d11 -lole32 -luuid -ldxguid \
    || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() { if printf '%s\n' "$out" | grep -qx "$1"; then pass "$2"; else fail "$2"; fi; }
check 'ForeignTargetReads=0' "a list another context made does not read back as the target (as on Windows)"
check 'EndDraw=0x00000000' "a context draws into a list another context made"
check 'Close=0x00000000' "the list closes after that context is gone"
check 'ForeignLastRelease=1' "the program's last Release leaves it alive (the drawing context's reference is dropped later)"
check 'Effect=0x00000000' "an effect is made"
check 'DrawInput=0x00000000' "and draws the released list as its input"
check 'UndrawnContextGone=0' "one set on another context and never drawn into goes with that context"
check 'NoTargetDraw=1' "a context with no target draws without crashing"
check 'OwnLastRelease=0' "a list its own context drew into is freed at its last Release"
check 'FactoryLastRelease=0' "the lists kept a while go with the device (nothing holds the factory)"
check 'Done=1' "the probe finished"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
