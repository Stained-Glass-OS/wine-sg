#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The virtual desktop as an XDND target the way a window manager's frames are
# (patches/sg/7788): Linux programs' windows framed in Wine windows (sg_embed.c)
# in a shell desktop on Xvfb, the taskbar fed by a stand-in sg-lockctl:
#
#   own window    a drag inside a framed Linux program finds the program's own
#                 window as Qt looks for it (the first XdndAware window from the
#                 root): its own drag (David 2026-11-15: a clip onto Kdenlive's
#                 timeline did nothing -- the drag was offered to Wine's desktop)
#   other program a drag from one framed Linux program onto another reaches it:
#                 its drop gets the file's URI, the source XdndFinished
#   Wine window   onto a Wine program's OLE drop target: Wine takes it, as before
#   desktop       onto the desktop's own picture: Wine's desktop takes it (the
#                 file lands in the Desktop folder)
#
#   WINE=/opt/wine-sg/bin/wine test/xdndembed-gate.sh
#   mutants: SG_MUTANT_DND_DESKTOP_AWARE (window.c), SG_MUTANT_DND_FRAME_AWARE (sg_embed.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DISP="${XDNDEMBED_DISPLAY:-:264}"
unset SESSION_MANAGER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: needs libx11-dev"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-xdndembed.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${WINEDEBUG:--all}" WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
PIDS=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    [ -n "${XVFB:-}" ] && kill "$XVFB" 2>/dev/null
    [ -n "${KEEP:-}" ] && { echo "kept $T"; return; }
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/native" "$HERE/xdndembed-native.c" -lX11 || { echo "FAIL  native program did not build"; exit 1; }
"$MINGW" -municode -mwindows -O2 -o "$T/xdndsrc-probe.exe" "$HERE/xdndsrc-probe.c" -lole32 -luuid -lshell32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }

Xvfb "$DISP" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
export DISPLAY="$DISP"
for i in $(seq 50); do xdotool getmouselocation >/dev/null 2>&1 && break; sleep 0.1; done

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/xdndsrc-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

mkdir -p "$T/files"
printf 'clip\n' > "$T/files/clip-one.txt"
LA="$T/a.log"; LB="$T/b.log"; : > "$LA"; : > "$LB"
"$T/native" "$LA" "Linux A" 60 60 320 220 "$T/files/clip-one.txt" &
PIDS="$PIDS $!"
"$T/native" "$LB" "Linux B" 480 60 320 220 &
PIDS="$PIDS $!"
for i in $(seq 50); do grep -q READY "$LA" && grep -q READY "$LB" && break; sleep 0.1; done
XA=$(sed -n 's/^READY /0x/p' "$LA"); XB=$(sed -n 's/^READY /0x/p' "$LB")
[ -n "$XA" ] && [ -n "$XB" ] || { fail "native windows did not come up"; exit 1; }
printf '%d shown - Native\tLinux A\n%d shown - Native\tLinux B\nEND\n' "$((XA))" "$((XB))" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
PIDS="$PIDS $!"
framed() {
    _k=0
    while [ $_k -lt 120 ]; do
        case "$(xwininfo -id "$1" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')" in
            ""|"(the root window)"*) ;; *) return 0 ;; esac
        sleep 0.25; _k=$((_k + 1))
    done
    return 1
}
framed "$XA" && framed "$XB" && pass "both Linux windows are framed in Wine windows" \
    || { fail "the Linux windows were not framed"; exit 1; }

BLOG="$WINEPREFIX/drive_c/b.log"; : > "$BLOG"
XDNDSRC_LOG='C:\b.log' "$WINE" 'C:\xdndsrc-probe.exe' target 100 380 300 200 >/dev/null 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 150); do grep -q READY "$BLOG" 2>/dev/null && break; sleep 0.2; done
grep -q "REGISTER hr=0x0" "$BLOG" || { fail "the Wine drop target did not come up"; exit 1; }
sleep 2

# where things are now (the frames may have moved the Linux windows)
center() { xwininfo -id "$1" | awk '/Absolute upper-left X/ {x=$4} /Absolute upper-left Y/ {y=$4} /Width/ {w=$2} /Height/ {h=$2} END {print x + w/2, y + h/2}'; }
set -- $(center "$XA"); AX=$1; AY=$2
set -- $(center "$XB"); BX=$1; BY=$2
WT=$(xdotool search --name '^sg-xdnd-wine-target$' | head -1)
set -- $(center "$WT"); WX=$1; WY=$2

count() { grep -c "^$2" "$1"; }
# drag SX SY EX EY: from the Linux A window
drag() {
    before=$(count "$LA" SRCRESULT)
    xdotool mousemove "$1" "$2"; sleep 0.4
    xdotool mousedown 1; sleep 0.5
    xdotool mousemove $(( $1 + 12 )) $(( $2 + 8 )); sleep 0.3
    xdotool mousemove "$3" "$4"; sleep 0.4
    xdotool mousemove $(( $3 + 3 )) $(( $4 + 2 )); sleep 0.6
    xdotool mouseup 1
    for i in $(seq 60); do [ "$(count "$LA" SRCRESULT)" -gt "$before" ] && break; sleep 0.2; done
    sleep 0.5
    LAST=$(grep '^SRCRESULT' "$LA" | tail -1)
}

# 1. inside its own window
drag $(( AX - 60 )) $(( AY - 40 )) $(( AX + 50 )) $(( AY + 40 ))
if [ "$LAST" = "SRCRESULT own" ]; then pass "a drag inside a framed Linux program is its own ($(grep '^TARGET' "$LA" | tail -1))"
else fail "inside its own window: $LAST, targets: $(grep '^TARGET' "$LA" | tr '\n' ' ')"; fi

# 2. onto the other Linux program
drag "$AX" "$AY" "$BX" "$BY"
if grep -qxF "DROP file://$T/files/clip-one.txt" "$LB" && echo "$LAST" | grep -q 'finished=XdndActionCopy ok=1'; then
    pass "onto another framed Linux program: dropped there ($LAST)"
else fail "onto the other program: $LAST / $(tr '\n' ' ' < "$LB")"; fi

# 3. onto a Wine program's drop target
drag "$AX" "$AY" "$WX" "$WY"
if grep -q '^WINEDROP ole=0 files=Z:.*clip-one.txt$' "$BLOG" && echo "$LAST" | grep -q 'ok=1'; then
    pass "onto a Wine window: Wine's drop target took it ($LAST)"
else fail "onto a Wine window: $LAST / $(tr '\n' ' ' < "$BLOG")"; fi

# 4. onto the desktop's own picture
DESKDIR=$("$WINE" winepath -u 'C:\users\'"$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)"'\Desktop' 2>/dev/null | tr -d '\r')
drag "$AX" "$AY" 860 470
for i in $(seq 25); do [ -e "$DESKDIR/clip-one.txt" ] && break; sleep 0.2; done
if [ -e "$DESKDIR/clip-one.txt" ] && echo "$LAST" | grep -q 'ok=1'; then
    pass "onto the desktop: the file is in the Desktop folder ($LAST)"
else fail "onto the desktop: $LAST, Desktop folder: $(ls "$DESKDIR" 2>&1 | tr '\n' ' ')"; fi

[ "$RC" = 0 ] && echo "xdndembed: all checks passed"
exit $RC
