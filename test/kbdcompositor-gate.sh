#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's keyboard layouts under the real compositor, on both of Wine's display drivers (patches/sg/
# 9154, 9156, 9158): GetKeyboardLayoutList lists the compositor keymap's layouts in the person's order,
# Windows logo key + Space is an input language change (WM_INPUTLANGCHANGEREQUEST forward/backward) after
# which the keys are the new layout's, and a program in the foreground that activates one of them switches
# the keyboard -- the compositor is asked ("LAYOUT n" on its control socket: it owns the group, for its
# Xwayland and its Wayland clients alike).
#   x11:     winex11 on the compositor's own Xwayland, as every SG session runs Windows programs
#   wayland: winewayland as a Wayland client (SG_DISPLAY_PATH=wayland)
# sg-compositor's test build (meson -Dtest-tablet=true: a keyboard fed from a FIFO) runs headless with the
# keymap "us,de" and grp:win_space_toggle; test/kbdactivate-probe.c is the window.
#   WINE=... SG_COMPOSITOR_TEST=/path/to/build-pen/sg-compositor test/kbdcompositor-gate.sh [x11|wayland]
# The compositor starts an Xwayland of its own, which takes the first free X display: run the gate in a
# private mount namespace (a private /tmp and /tmp/.X11-unix), never beside a person's desktop.
# Mutants: SG_MUTANT_KBD_WL_ONE_GROUP, SG_MUTANT_KBD_WL_ACTIVATE_LOCAL (winewayland), SG_MUTANT_KBD_ACTIVATE_LOCAL
# (winex11).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
COMP="${SG_COMPOSITOR_TEST:-}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in python3 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -n "$COMP" ] && [ -x "$COMP" ] || { echo "SKIP: no sg-compositor test build (SG_COMPOSITOR_TEST)"; exit 77; }
DRIVERS="${1:-x11 wayland}"
T=$(mktemp -d /var/tmp/sg-kbdcompositor.XXXXXX)
CP=""; PP=""
cleanup() {
    [ -n "$PP" ] && kill "$PP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$CP" ] && kill -9 "$CP" 2>/dev/null
    [ "${KEEP:-0}" = 1 ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/kbdactivate-probe.c" -luser32 || { echo "FAIL  the probe did not build"; exit 1; }
unset DISPLAY
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

run() {
drv=$1
rm -f "$T/in" "$T/wl" "$T/dpy" "$T/ctl.sock" "$T/priv.sock"
mkfifo "$T/in"
( printf 'kplug\n' > "$T/in" ) &
XKB_DEFAULT_LAYOUT=us,de XKB_DEFAULT_VARIANT= XKB_DEFAULT_OPTIONS=grp:win_space_toggle \
    SG_TEST_TABLET_FIFO="$T/in" WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
    sh -c "echo \$DISPLAY > $T/dpy; echo \$WAYLAND_DISPLAY > $T/wl; exec sleep 600" >"$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -s "$T/wl" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
[ -s "$T/wl" ] || { echo "FAIL  $drv: the compositor did not start"; cat "$T/comp.log"; rc=1; return; }
export SG_LOCK_CONTROL="$T/ctl.sock"
if [ "$drv" = wayland ]; then export WAYLAND_DISPLAY="$(cat "$T/wl")"; unset DISPLAY
else export DISPLAY="$(cat "$T/dpy")" WAYLAND_DISPLAY=sg-gate-no-wayland; fi
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d "$drv" /f >/dev/null 2>&1
ctl() { python3 -c '
import socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.sendall(sys.argv[2].encode() + b"\n")
print(s.recv(700).decode().strip())' "$T/ctl.sock" "$1"; }
keys() { for k in "$@"; do case "$k" in s*) python3 -c "import time; time.sleep(${k#s}/1000)" ;;
         +*) printf 'key %s 1\n' "${k#+}" > "$T/in" ;; -*) printf 'key %s 0\n' "${k#-}" > "$T/in" ;; esac
         python3 -c "import time; time.sleep(0.06)"; done; }

# winex11 as an SG session runs it: the shell's desktop window (explorer's virtual desktop) on the
# compositor's Xwayland, the program's window in it
if [ "$drv" = x11 ]; then
    "$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
    "$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
    "$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
    sleep 3
else
    "$WINE" reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1
fi
timeout -s KILL 90 "$WINE" "$T/probe.exe" > "$T/out" 2>"$T/err" </dev/null & PP=$!
i=0; while ! grep -q '^READY' "$T/out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
sleep 2
# the window to the front with the keyboard, as the taskbar brings it (XACTIVATE): a headless compositor
# has had no click to focus it
ctl XWINDOWS > "$T/xwindows"
w=$(ctl XWINDOWS | awk -F'\t' '$2 == "sgkbdactivate" || $2 ~ / Desktop/ { split($1, a, " "); print a[1] }' | head -n 1)
[ -n "$w" ] && ctl "XACTIVATE $w" >/dev/null
sleep 1
SWITCH="+125 +57 -57 -125 s400"
keys +21 -21 s400                    # Y
keys $SWITCH +21 -21 s400            # to German: Y
keys $SWITCH +21 -21 s400            # back to US: Y
keys +16 -16 s800                    # Q: the program activates German
g=$(ctl KEYBOARD | sed -n 's/^OK group=\([0-9]*\).*/\1/p')
keys +21 -21 s400 +18 -18 s400       # Y, E: done
i=0; while ! grep -q '^DONE\|^TIMEOUT' "$T/out" && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
tr -d '\r' < "$T/out" > "$T/log"
sed 's/^/      /' "$T/log"
grep -v '^DOWN 5b\|^DOWN 20\|^CHAR 0020\|^READY' "$T/log" | tr '\n' '|' > "$T/seq"
check() { case "$(cat "$T/seq")" in *"$2"*) echo "PASS  $drv: $1" ;; *) echo "FAIL  $drv: $1: want '$2'"; rc=1 ;; esac; }
grep -q '^READY' "$T/log" || { echo "FAIL  $drv: the probe's window did not start"; tail -20 "$T/comp.log"; rc=1; return; }
check "GetKeyboardLayoutList: the session's two layouts, US then German" "LIST 2 04090409 04070409|"
check "US: the Y key is VK_Y, y" "|DOWN 59 15|CHAR 0079|"
check "Windows logo key + Space: WM_INPUTLANGCHANGEREQUEST German, forward; then the Y key is VK_Z, z" \
    "REQ 3 04070409|CHANGE 04070409|DOWN 5a 15|CHAR 007a|"
check "again: WM_INPUTLANGCHANGEREQUEST US, backward; the Y key is VK_Y" "REQ 5 04090409|CHANGE 04090409|DOWN 59 15|CHAR 0079|"
check "ActivateKeyboardLayout(German) in the foreground" "|ACT 04090409 04070409|"
if [ "$g" = 1 ]; then echo "PASS  $drv: ...switches the compositor's keyboard to German (group 1)"; else echo "FAIL  $drv: ...the compositor's keyboard is in group '$g', want 1"; rc=1; fi
check "...and the Y key is then VK_Z, z" "|ACT 04090409 04070409|DOWN 5a 15|CHAR 007a|"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
kill "$PP" 2>/dev/null; kill -9 "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; PP=""; CP=""
}
rc=0
for d in $DRIVERS; do run "$d"; done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
