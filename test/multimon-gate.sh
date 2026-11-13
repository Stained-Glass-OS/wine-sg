#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's desktop over every monitor (patches/sg/7749). David 2026-11-12:
# with two monitors the desktop came up on one and the sign-in screen stayed on
# the other. The session's desktop (explorer /desktop=shell, HKCU\Software\Wine\
# Explorer HostMonitors=Y) now shows programs the host's monitors as they are,
# not one virtual monitor, and its window reaches all of them.
#
# A headless sg-compositor with two outputs (Xwayland reports them side by
# side, as a laptop and a monitor) runs the desktop and a probe in it:
#
#   - programs see two monitors, each where it is, the first primary
#   - the desktop's X window is both of them wide
#   - a window maximized on either monitor fills that monitor, not both
#   - without HostMonitors, one virtual monitor (Wine's own virtual desktop)
#
#   WINE=/opt/wine-sg/bin/wine test/multimon-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor>, or SG_COMPOSITOR_SRC=<checkout> to build one
# Mutants: SG_MUTANT_ONE_VIRTUAL_MONITOR, SG_MUTANT_SPAN_CLIPPED (win32u/sysparams.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v xwininfo >/dev/null || { echo "SKIP: needs mingw-w64 and xwininfo"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"

T=$(mktemp -d /var/tmp/sg-multimon.XXXXXX)
# (the compositor waits for its child, the sleep, before it goes)
stop() { [ -n "$CP" ] && { pkill -P "$CP" 2>/dev/null; kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; CP=""; }
cleanup() { "$WINESERVER" -k 2>/dev/null; stop; [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM

if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
elif [ -d "$SRC" ]; then
    meson setup "$T/comp" "$SRC" -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { tail -20 "$T/meson.log" "$T/ninja.log"; echo "SKIP: cannot build the compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
else
    echo "SKIP: no sg-compositor (SG_COMPOSITOR_BUILD or SG_COMPOSITOR_SRC)"; exit 77
fi
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/multimon-probe.exe" "$HERE/multimon-probe.c" -luser32 -lgdi32 ||
    { fail "probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

# a headless compositor with two outputs (1280x720 each, side by side) whose child says its X display
start() {
    rm -f "$T/disp"
    XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
        "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
        sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
    CP=$!
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
    [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
    sleep 1
}
# desktop OUT: the shell's desktop, the probe in it, until the probe is done
desktop() {
    rm -f "$T/$1"
    timeout -s KILL 180 "$WINE" explorer /desktop=shell,2560x720 'C:\multimon-probe.exe' "C:\\$1" >/dev/null 2>&1 &
    i=0; while ! grep -q '^ready' "$WINEPREFIX/drive_c/$1" 2>/dev/null && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
    tr -d '\r' < "$WINEPREFIX/drive_c/$1" > "$T/$1" 2>/dev/null
    xwininfo -root -children 2>/dev/null | grep 'Wine Desktop' > "$T/$1.x"
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
}
start
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
cp "$T/multimon-probe.exe" "$WINEPREFIX/drive_c/"
"$WINESERVER" -w
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    2560x720) ;;
    *) echo "SKIP: the compositor did not make two 1280x720 outputs: $(xwininfo -root | grep -E 'Width|Height' | tr -d ' \n')"; exit 77 ;;
esac

# --- the shell's desktop over both monitors ---
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1; "$WINESERVER" -w
desktop span.txt
grep -q '^monitors 2 virtual 0 0 2560 720' "$T/span.txt" && pass "programs see two monitors, the virtual screen both ($(head -1 "$T/span.txt"))" ||
    fail "not two monitors: $(head -3 "$T/span.txt" | tr '\n' ' ')"
grep -q '^monitor [^ ]* 0 0 1280 720 primary' "$T/span.txt" && grep -q '^monitor [^ ]* 1280 0 2560 720 -' "$T/span.txt" &&
    pass "each where it is, the first primary" || fail "monitors: $(grep '^monitor' "$T/span.txt" | tr '\n' ' ')"
grep -q '2560x720+0+0' "$T/span.txt.x" && pass "the desktop's X window is both monitors wide ($(sed 's/.*) *//' "$T/span.txt.x"))" ||
    fail "the desktop's X window: $(cat "$T/span.txt.x")"
grep -q '^max 0 [0-9]* [0-9]* 12[0-9][0-9] ' "$T/span.txt" && grep -q '^max 1 128[0-9] [0-9]* 25[0-9][0-9] ' "$T/span.txt" &&
    pass "a window maximized on either monitor fills that one ($(grep '^max' "$T/span.txt" | tr '\n' ' '))" ||
    fail "maximized: $(grep '^max' "$T/span.txt" | tr '\n' ' ')"

# --- without HostMonitors: Wine's own virtual desktop, one monitor ---
"$WINE" reg delete 'HKCU\Software\Wine\Explorer' /v HostMonitors /f >/dev/null 2>&1; "$WINESERVER" -w
desktop one.txt
grep -q '^monitors 1 ' "$T/one.txt" && pass "without HostMonitors: one virtual monitor, as Wine has it ($(head -1 "$T/one.txt"))" ||
    fail "without HostMonitors: $(head -1 "$T/one.txt")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
