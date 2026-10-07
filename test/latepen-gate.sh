#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A pen that comes after a program started, and goes again (patches/sg/1420):
# a Surface's pen device appears late (a Bluetooth pen, iptsd starting after
# the session, the pen out of range at boot), and a program already running
# must take it -- and lose it cleanly when it goes. sg-compositor's test build
# (-Dtest-tablet=true, SG_TEST_TABLET_LATE: the tablet arrives at a "plug"
# line and goes at "unplug"; 0.2.0+sg46) makes Xwayland add, disable and
# enable its pen devices as a real tablet's arrival does. test/latepen-probe.c
# is the program, started before the tablet comes, plainly and in the
# shell's own desktop (as in a session).
#
#  1. Before the tablet: no pen in GetPointerDevices, Wintab has no device.
#  2. Plugged in: GetPointerDevices lists the pen, the window gets
#     WM_POINTERDEVICECHANGE (PDC_ARRIVAL; RegisterPointerDeviceNotifications),
#     a stroke is WM_POINTERDOWN/UPDATE of PT_PEN with its pressure, and
#     Wintab has the tablet: a context opens and WT_PACKETs carry the pressure.
#  3. Unplugged in the middle of a stroke: the pen's pointer goes up and
#     leaves (WM_POINTERUP, WM_POINTERLEAVE), PDC_REMOVAL, no pen listed.
#  4. Plugged in again: the pen and its pressure are back.
#
#   WINE=<build>/wine WINESERVER=<build>/server/wineserver test/latepen-gate.sh
#   SG_COMPOSITOR_SRC=<sg-compositor checkout, 0.2.0+sg46 or later>
#   SG_COMPOSITOR_BUILD=<its test build> (else built here)
#
# Mutants (each must fail it): SG_MUTANT_LATEPEN_NO_NOTIFY (no
# WM_POINTERDEVICECHANGE; win32u input.c), LATEPEN_WINTAB_ONCE (Wintab's
# tablet read once; wintab32 context.c), LATEPEN_NO_LEAVE (a pen gone stays
# in range over its window; winex11 mouse.c), NO_PEN_HOTPLUG (wine-sg 1150's
# late pen; winex11 mouse.c). LATEPEN_KEEP=1 keeps the logs.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xwayland meson ninja "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
grep -q '"unplug"' "$SRC/seat.c" 2>/dev/null || { echo "SKIP: no sg-compositor 0.2.0+sg46 or later at $SRC"; exit 77; }

T=$(mktemp -d /var/tmp/sg-latepen.XXXXXX)
stop() { [ -n "$CP" ] && kill -9 "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; CP=""; }
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    stop; [ -n "${LATEPEN_KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM

if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
else
    meson setup "$T/comp" "$SRC" -Dtest-tablet=true -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { tail -20 "$T/meson.log" "$T/ninja.log"; echo "SKIP: cannot build the test compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
fi
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/latepen-probe.exe" "$HERE/latepen-probe.c" -luser32 -lgdi32 ||
    { fail "probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkfifo "$T/pen"
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
# Wine's programs find no Wayland socket (X11 only)
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG="${LATEPEN_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

# start: a headless compositor (1280x720) whose tablet comes only at "plug"
start() {
    rm -f "$T/disp"
    env SG_TEST_TABLET_LATE=1 XDG_RUNTIME_DIR="$T/comp-xdg" SG_TEST_TABLET_FIFO="$T/pen" WLR_BACKENDS=headless \
        WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
        sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
    CP=$!
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
    [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
    sleep 1
}
finish() { "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null; stop; }
# a real device's events come in frames of their own: one line per write
feed() { for l in "$@"; do printf '%s\n' "$l" > "$T/pen"; sleep 0.25; done; sleep 0.6; }
mark() { wc -l < "$1" > "$T/mark"; }
since() { tail -n +"$(( $(cat "$T/mark") + 1 ))" "$1" | tr -d '\r'; }
# wait_for LOG PATTERN SECONDS: PATTERN in LOG since the mark
wait_for() {
    i=0; while ! since "$1" | grep -q "$2" && [ $i -lt $(( $3 * 4 )) ]; do sleep 0.25; i=$((i + 1)); done
    since "$1" | grep -q "$2"
}

start
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINESERVER" -w
finish

# run WHERE: the whole walk, the probe started before the tablet comes;
# WHERE "plain" (its own top-level window) or "shell" (the shell's desktop)
run() {
    where=$1
    start
    if [ "$where" = shell ]; then
        "$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
        "$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x720 /f >/dev/null 2>&1
        "$WINESERVER" -w
        WINEDEBUG="${LATEPEN_DEBUG:-err+all}" "$WINE" explorer /desktop=shell,1280x720 > "$T/explorer.out" 2>&1 &
        i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
        sleep 3
    fi
    L=$T/$where.log
    LATEPEN_WINTAB=1 timeout -s KILL 600 "$WINE" "$T/latepen-probe.exe" > "$L" 2>"$L.err" &
    i=0; while ! grep -q '^ready' "$L" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
    grep -q '^ready' "$L" || { fail "$where: the probe did not start: $(head -c 300 "$L") $(tail -c 300 "$L.err")"; finish; return; }
    sleep 1

    # 1. before the tablet
    grep -q '^devices pens=0 ' "$L" && grep -q '^wintab devices=0' "$L" \
        && pass "$where: before the tablet, no pen ($(grep '^devices' "$L" | head -1); $(grep '^wintab devices' "$L" | head -1))" \
        || fail "$where: before the tablet: $(grep -E '^(devices|wintab)' "$L" | tr '\n' ' ')"

    # 2. plugged in
    mark "$L"
    feed plug
    wait_for "$L" '^devices pens=[1-9]' 10 && pass "$where: plugged in, GetPointerDevices lists the pen ($(since "$L" | grep '^devices' | tail -1))" \
        || fail "$where: no pen listed after the tablet came: $(since "$L" | grep '^devices' | tr '\n' ' ')"
    since "$L" | grep -q '^devchange arrival' && pass "$where: WM_POINTERDEVICECHANGE says it arrived" \
        || fail "$where: no WM_POINTERDEVICECHANGE (PDC_ARRIVAL): $(since "$L" | grep '^devchange' | tr '\n' ' ')"
    wait_for "$L" '^wintab open=1' 5 && pass "$where: Wintab has the tablet now ($(since "$L" | grep '^wintab devices' | tail -1)); a context opens" \
        || fail "$where: Wintab: $(since "$L" | grep '^wintab' | tr '\n' ' ')"
    sleep 1
    mark "$L"
    feed "in 0.5 0.5" "move 0.5 0.5" "pressure 0.1" down "pressure 0.7" "move 0.51 0.5" up "move 0.53 0.5" out
    since "$L" > "$T/$where.stroke1"
    grep '^pointer down' "$T/$where.stroke1" | grep -q 'type=3 pressure=102 ' && grep '^pointer update' "$T/$where.stroke1" | grep -q 'type=3 pressure=717 ' \
        && pass "$where: the late pen's stroke is PT_PEN with its pressure (102, then 717)" \
        || fail "$where: the late pen's stroke: $(grep -c '^pointer' "$T/$where.stroke1") pointer messages; $(grep '^pointer' "$T/$where.stroke1" | cut -c1-40 | sort | uniq -c | tr '\n' ' ')"
    grep -q '^wintab packet pressure=[1-9]' "$T/$where.stroke1" \
        && pass "$where: and Wintab's packets carry its pressure ($(grep -o '^wintab packet pressure=[0-9]*' "$T/$where.stroke1" | sort -u | tail -1))" \
        || fail "$where: no Wintab packet with pressure: $(grep -c '^wintab packet' "$T/$where.stroke1") packets"

    # 3. unplugged in the middle of a stroke
    mark "$L"
    feed "in 0.5 0.5" "move 0.5 0.5" "pressure 0.5" down "move 0.51 0.5"
    feed unplug
    sleep 1
    since "$L" > "$T/$where.gone"
    grep -q '^devices pens=0 ' "$T/$where.gone" && pass "$where: unplugged, no pen listed" \
        || fail "$where: after the tablet went: $(grep '^devices' "$T/$where.gone" | tr '\n' ' ')"
    grep -q '^devchange removal' "$T/$where.gone" && pass "$where: WM_POINTERDEVICECHANGE says it went" \
        || fail "$where: no PDC_REMOVAL: $(grep '^devchange' "$T/$where.gone" | tr '\n' ' ')"
    sed -n '/^pointer down/,$p' "$T/$where.gone" | grep -q '^pointer up type=3' && sed -n '/^pointer down/,$p' "$T/$where.gone" | grep -q '^pointer leave type=3' \
        && pass "$where: the pen gone mid-stroke goes up and leaves (WM_POINTERUP, WM_POINTERLEAVE)" \
        || fail "$where: the pen gone mid-stroke: $(grep '^pointer' "$T/$where.gone" | cut -c1-30 | uniq -c | tr '\n' ' ')"

    # 4. plugged in again
    mark "$L"
    feed plug
    wait_for "$L" '^devices pens=[1-9]' 10 || true
    sleep 1
    feed "in 0.5 0.5" "move 0.5 0.5" "pressure 0.1" down "pressure 0.7" "move 0.51 0.5" up out
    since "$L" > "$T/$where.stroke2"
    grep -q '^devices pens=[1-9]' "$T/$where.stroke2" && grep '^pointer update' "$T/$where.stroke2" | grep -q 'type=3 pressure=717 ' \
        && pass "$where: plugged in again, the pen and its pressure are back" \
        || fail "$where: plugged in again: $(grep '^devices' "$T/$where.stroke2" | tr '\n' ' '); $(grep '^pointer' "$T/$where.stroke2" | cut -c1-40 | sort | uniq -c | tr '\n' ' ')"
    grep -q '^wintab packet pressure=[1-9]' "$T/$where.stroke2" && pass "$where: and in Wintab's packets" \
        || fail "$where: plugged in again, no Wintab packet with pressure: $(grep -c '^wintab packet' "$T/$where.stroke2") packets"
    finish
}

run plain
run shell

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
