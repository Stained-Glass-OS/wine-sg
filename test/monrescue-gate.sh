#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window on a display that is unplugged comes to a remaining one, and a window on
# a display that stays stays on it (patches/sg/9972), as on Windows. QA VM ONLY:
# it starts a headless sg-compositor (never on a machine someone works on).
#
# Windows keeps each window on its display when the displays change: its
# coordinates start at the main display, so a window on a display that stays
# moves with that display's rectangle, and one on a display that is gone is
# brought to the main display. Wine kept every rectangle: after the main
# display of two was unplugged, the windows on the other one were off the
# screen (ledger i0645). A headless compositor with two outputs, 2560x1440
# (the main display) and 1920x1080 right of it, the shell's desktop over both:
#   1. a window on the 1920x1080 display at 2700,100; the main display turned
#      off: the window is on the remaining display, at the same place on it
#      (140,100; it stayed at 2700, off the screen, before)
#   2. a window on the main display at 1500,900 (400x300), the main display
#      turned off: it is on the remaining display, whole (it hung off its
#      bottom before)
#
#   WINE=/opt/wine-sg/bin/wine test/monrescue-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor> (default: the installed one)
# Mutant: SG_MUTANT_RESCUE_NONE (programs/explorer/desktop.c) fails 1 and 2.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
# SG_PREBUILT=<dir with probe.exe and fake-lockctl>: a QA VM has no compilers
need="xwininfo wlr-randr"; [ -z "${SG_PREBUILT:-}" ] && need="$need cc $MINGW"
for t in $need; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
COMP=${SG_COMPOSITOR_BUILD:+$SG_COMPOSITOR_BUILD/sg-compositor}
COMP=${COMP:-$(command -v sg-compositor)}
[ -x "$COMP" ] || { echo "SKIP: no sg-compositor"; exit 77; }
# a QA VM's, never a desktop someone works at
if [ -z "${SG_QA_VM:-}" ]; then echo "SKIP: QA VM only (SG_QA_VM=1)"; exit 77; fi
RC=0; CP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-monrescue.XXXXXX)
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$CP" ] && { pkill -P "$CP" 2>/dev/null; kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }
    [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
if [ -n "${SG_PREBUILT:-}" ]; then
    cp "$SG_PREBUILT/fake-lockctl" "$SG_PREBUILT/probe.exe" "$T/" || { echo "FAIL  no prebuilt probe"; exit 1; }
else
    cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
    TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/pmscale-probe.c" -lshcore -luser32 || { echo "FAIL  probe did not build"; exit 1; }
fi

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/run"; chmod 700 "$T/comp-xdg" "$T/run"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
    sh -c "echo \$WAYLAND_DISPLAY > $T/wl; echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
case "$DISPLAY" in ""|:0) echo "FAIL  no display from the compositor"; cat "$T/comp.log"; exit 1 ;; esac
# the two outputs: 2560x1440, and 1920x1080 right of it
WL=$(cat "$T/wl")
OUTS=$(XDG_RUNTIME_DIR="$T/comp-xdg" WAYLAND_DISPLAY=$WL wlr-randr | awk '/^[^ ]/ {print $1}')
O1=$(echo "$OUTS" | sed -n 1p); O2=$(echo "$OUTS" | sed -n 2p)
XDG_RUNTIME_DIR="$T/comp-xdg" WAYLAND_DISPLAY=$WL wlr-randr --output "$O1" --custom-mode 2560x1440 --pos 0,0 \
    --output "$O2" --custom-mode 1920x1080 --pos 2560,0 >>"$T/comp.log" 2>&1
export XDG_RUNTIME_DIR="$T/run"
sleep 2
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    4480x1440) ;;
    *) echo "SKIP: the compositor did not make the two outputs ($(xwininfo -root | grep -E 'Width|Height' | tr -d '\n'))"; exit 77 ;;
esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 120 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Display' /v AutoLogPixels /t REG_DWORD /d 120 /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T"
printf 'END\n' > "$T/list"
"$WINE" explorer /desktop=shell,4480x1440 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
# the DPI of the monitor at X (screen) in "list" output
mondpi() { echo "$2" | awk -v x="$1" '$1 == "MON" && $3 <= x && x < $5 {print $7; exit}'; }

probe pin 'Rescue A' 2700 100 400 300 60 >/dev/null 2>&1 &
probe pin 'Rescue B' 1500 900 400 300 60 >/dev/null 2>&1 &
i=0; while [ "$(probe where 'Rescue B' | cut -d' ' -f1)" != AT ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
A0=$(probe where 'Rescue A'); B0=$(probe where 'Rescue B')
XDG_RUNTIME_DIR="$T/comp-xdg" WAYLAND_DISPLAY=$WL wlr-randr --output "$O1" --off >>"$T/comp.log" 2>&1
i=0; while [ "$(probe list | grep -c '^MON')" != 1 ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
L=$(probe list); A=$(probe where 'Rescue A'); B=$(probe where 'Rescue B')
set -- $(echo "$L" | awk '$1 == "MON" {print $3, $4, $5, $6; exit}')
ML=${1:-0} MT=${2:-0} MR=${3:-0} MB=${4:-0}
set -- $A
[ "$1" = AT ] && [ "$2" = $((ML + 140)) ] && [ "$3" = $((MT + 100)) ] \
    && pass "a window on the display that stays: at the same place on it ($A; before $A0)" \
    || fail "the window on the remaining display: want AT $((ML + 140)) $((MT + 100)) ..., got '$A' (before: $A0; monitors: $(echo $L))"
set -- $B
[ "$1" = AT ] && [ "$2" -ge "$ML" ] && [ "$3" -ge "$MT" ] && [ "$4" -le "$MR" ] && [ "$5" -le "$MB" ] \
    && pass "a window on the display unplugged: whole on the remaining one ($B; before $B0)" \
    || fail "the window of the display unplugged: want it inside $ML,$MT-$MR,$MB, got '$B' (before: $B0)"
exit $RC
