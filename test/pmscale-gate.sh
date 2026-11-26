#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Each monitor has its own scale, as on Windows (patches/sg/9971). QA VM ONLY:
# it starts a headless sg-compositor (never on a machine someone works on).
#
# David's GitHub report (2026-11-25): a 1440p main display at 125% beside a
# 1080p one; unplugging the 1440p one turned every window from 125% to 100%,
# and back when it came back. LogPixels was every monitor's scale, and the
# automatic scale follows the main display. Now each monitor has the scale
# kept for it (HKCU\Control Panel\Desktop\PerMonitorSettings, steps from the
# recommended one), else the recommended one for its size; a window moved to
# a monitor of another scale is told (WM_DPICHANGED) or scaled by Wine.
#
# A headless compositor with two outputs, 2560x1440 and 1920x1080 side by
# side, the shell's desktop over both (HostMonitors), LogPixels 120 written by
# the automatic scale (AutoLogPixels 120):
#   1. the 2560x1440 monitor 120 DPI, the 1920x1080 one 96 (both 120 before)
#   2. a per-monitor v2 window moved onto the 1080p monitor gets WM_DPICHANGED 96,
#      back 120
#   3. DisplayConfigGetDeviceInfo (Windows' type -3): the 1080p monitor's scale
#      is its recommended one (0), 0 to +2 offered
#   4. DisplayConfigSetDeviceInfo (type -4) +1: the 1080p monitor 120 DPI, kept in
#      PerMonitorSettings; the main one stays 120. Back to 0: 96 again
#   5. a scale picked for all monitors before (LogPixels 144, no settings of
#      each monitor's) is every monitor's, as it was
#
#   WINE=/opt/wine-sg/bin/wine test/pmscale-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor> (default: the installed one)
# Mutants: SG_MUTANT_SCALE_LOGPIXELS_ONLY fails 1, 4; SG_MUTANT_MONITOR_DPI_NO_MOVE fails 2.
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
T=$(mktemp -d /var/tmp/sg-pmscale.XXXXXX)
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

L=$(probe list)
d1=$(mondpi 100 "$L"); d2=$(mondpi 2700 "$L")
[ "$d1" = 120 ] && [ "$d2" = 96 ] && pass "each monitor its recommended scale: 2560x1440 120 DPI, 1920x1080 96" \
    || fail "want 120 and 96, got '$d1' and '$d2' ($(echo $L))"

M=$(probe move 2700 100)
echo "$M" | grep -q '^DPICHANGED 96$' && echo "$M" | grep -q '^B 96 ' && echo "$M" | grep -q '^DPICHANGED 120$' &&
    echo "$M" | grep -q '^C 120 ' && pass "a per-monitor aware window moved onto the 1080p monitor: WM_DPICHANGED 96, back 120" \
    || fail "moved between the monitors: $(echo $M)"

# monitor index of the 1080p one in the probe's list
N2=$(echo "$L" | awk '$1 == "MON" && $3 <= 2700 && 2700 < $5 {print $2; exit}')
G=$(probe get "$N2")
[ "$G" = "SCALE $N2 0 0 2" ] && pass "DisplayConfigGetDeviceInfo: the 1080p monitor at its recommended scale, up to +2 ($G)" \
    || fail "DisplayConfigGetDeviceInfo: want 'SCALE $N2 0 0 2', got '$G'"

S=$(probe set "$N2" 1)
d1=$(mondpi 100 "$S"); d2=$(mondpi 2700 "$S")
K=$("$WINE" reg query 'HKCU\Control Panel\Desktop\PerMonitorSettings' /s 2>/dev/null | tr -d '\r' | grep -c 'DpiValue')
if echo "$S" | grep -q "^SET $N2 0$" && [ "$d1" = 120 ] && [ "$d2" = 120 ] && [ "$K" -ge 1 ]; then
    S=$(probe set "$N2" 0); d2b=$(mondpi 2700 "$S")
    [ "$d2b" = 96 ] && pass "DisplayConfigSetDeviceInfo: the 1080p monitor +1 is 120 DPI, kept for it; the main one stays 120; 0 again is 96" \
        || fail "set back to its recommended scale: want 96, got '$d2b' ($(echo $S))"
else
    fail "DisplayConfigSetDeviceInfo +1: want SET 0, 120 and 120, kept; got $(echo $S) (kept: $K)"
fi

# a scale picked for all monitors before they had their own
"$WINE" reg delete 'HKCU\Control Panel\Desktop\PerMonitorSettings' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 144 /f >/dev/null 2>&1
probe settingchange >/dev/null; sleep 2
L=$(probe list)
d1=$(mondpi 100 "$L"); d2=$(mondpi 2700 "$L")
[ "$d1" = 144 ] && [ "$d2" = 144 ] && pass "a scale picked for all monitors before (LogPixels 144) is every monitor's" \
    || fail "LogPixels 144 picked for all: want 144 and 144, got '$d1' and '$d2'"
exit $RC
