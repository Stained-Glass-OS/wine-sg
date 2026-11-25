#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The horizontal wheel's message is the vertical one's twin (patches/sg/9510).
#
# Windows has no non-client wheel message, and both WM_MOUSEWHEEL and
# WM_MOUSEHWHEEL carry the pointer in SCREEN coordinates, wherever over the
# window it is. win32u exempted WM_MOUSEWHEEL alone: WM_MOUSEHWHEEL came in
# client coordinates (Chromium and every program ScreenToClient the lParam
# of both, so a tilt or a touchpad's sideways scroll landed at the wrong
# point), and over the title bar it became message 0x28C, which nothing
# handles. The probe window logs what it gets for the X wheel buttons 4-7
# (winex11: 4/5 vertical, 6/7 horizontal) over its client area and caption.
#
#   WINE=/opt/wine-sg/bin/wine test/hwheel-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xdotool >/dev/null || { echo "SKIP: needs xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-hwheel.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/hwheel-probe.exe" "$HERE/hwheel-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/hwheel-probe.exe" "$C/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 > "$T/explorer.out" 2>&1 &
i=0; while ! xdotool search --name 'shell - Wine Desktop' >/dev/null 2>&1 && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done

LOG="$C/hwheel.log"
(cd "$C" && "$WINE" hwheel-probe.exe 'C:\hwheel.log' >/dev/null 2>&1 &)
i=0; while ! grep -q '^RECT' "$LOG" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
grep -q '^RECT' "$LOG" || { fail "the probe window did not open"; exit 1; }
sleep 2
set -- $(grep '^RECT' "$LOG" | tr -d '\r')
WL=$2 WT=$3 CL=$6 CT=$7 CR=$8 CB=$9
# the desktop is the X screen at 0,0: screen coordinates are X root coordinates
CX=$(( (CL + CR) / 2 )); CY=$(( (CT + CB) / 2 ))
NX=$(( WL + 150 )); NY=$(( (WT + CT) / 2 ))
[ "$CT" -gt "$WT" ] || { fail "the probe window has no caption ($WT/$CT)"; exit 1; }

# wheel BUTTON at X,Y; prints the probe's lines it caused
wheel() {
    n=$(grep -c '^MSG' "$LOG")
    xdotool mousemove "$1" "$2"; sleep 0.4; xdotool click "$3"; sleep 0.8
    tail -n +"$((n + 2))" "$LOG" | tr -d '\r' | grep '^MSG'
}
check() {   # WHAT X Y BUTTON MSG DELTA
    got=$(wheel "$2" "$3" "$4")
    want="MSG $5 $2 $3 $6"
    if [ "$got" = "$want" ]; then pass "$1: $want"; else fail "$1: want '$want', got '$(echo $got)'"; fi
}
check "vertical wheel over the client area (screen coordinates)"   "$CX" "$CY" 5 020a -120
check "horizontal wheel over the client area (screen coordinates)" "$CX" "$CY" 7 020e 120
check "horizontal wheel left over the client area"                 "$CX" "$CY" 6 020e -120
check "vertical wheel over the title bar (no non-client form)"     "$NX" "$NY" 4 020a 120
check "horizontal wheel over the title bar (no non-client form)"   "$NX" "$NY" 7 020e 120
grep -q '^MSG 028c' "$LOG" && fail "message 0x28C (a non-client WM_MOUSEHWHEEL) was delivered" \
    || pass "no message 0x28C"
exit $RC
