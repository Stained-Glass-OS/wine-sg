#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Drags in a framed Qt program, with real Qt (PyQt6), shaped like Kdenlive
# (David 2026-11-15, after 7787/7788: "Drag into kdenlive does not seem to work.
# kdenlive files to timeline does not work"):
#   bin -> timeline   Qt's own drag inside the program (a private type): it drops
#   Wine window       files dragged from a Wine window (DoDragDrop, CF_HDROP) onto
#                     the timeline arrive as text/uri-list (7787)
#   File Explorer     the real File Explorer's list view as the source (the
#                     shell's data object: HDROP rendered on demand, IDLists):
#                     a file dragged onto the timeline arrives, and is not moved
#
#   WINE=/opt/wine-sg/bin/wine test/xdndqt-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DISP="${XDNDQT_DISPLAY:-:265}"
unset SESSION_MANAGER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: needs libx11-dev"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-xdndqt.XXXXXX)
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
python3 -c "import PyQt6.QtWidgets" 2>/dev/null || { echo "SKIP: needs python3-pyqt6"; exit 77; }
"$MINGW" -municode -mwindows -O2 -o "$T/xdndsrc-probe.exe" "$HERE/xdndsrc-probe.c" -lole32 -luuid -lshell32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/drag-probe.exe" "$HERE/drag-probe.c" -lcomctl32 || { echo "FAIL  drag-probe did not build"; exit 1; }

Xvfb "$DISP" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
export DISPLAY="$DISP"
for i in $(seq 50); do xdotool getmouselocation >/dev/null 2>&1 && break; sleep 0.1; done

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/xdndsrc-probe.exe" "$T/drag-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

mkdir -p "$T/files" "$T/clips"
printf 'clip\n' > "$T/files/clip-one.txt"
printf 'clip\n' > "$T/clips/clip-two.txt"
L="$T/qt.log"; : > "$L"
QT_QPA_PLATFORM=xcb python3 "$HERE/xdndqt-app.py" "$L" &
PIDS="$PIDS $!"
for i in $(seq 100); do grep -q READY "$L" && break; sleep 0.1; done
XQ=$(sed -n 's/^READY //p' "$L")
[ -n "$XQ" ] || { fail "the Qt program did not come up"; exit 1; }
XQ=$(printf '0x%x' "$XQ")
# Qt's window id is its client window; the top-level the compositor lists is it
printf '%d shown - python3\tQt Timeline\nEND\n' "$((XQ))" > "$T/list"
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
framed "$XQ" && pass "the Qt program is framed in a Wine window" || { fail "the Qt program was not framed"; exit 1; }
sleep 2
geo() { xwininfo -id "$1" | awk '/Absolute upper-left X/ {x=$4} /Absolute upper-left Y/ {y=$4} /Width/ {w=$2} /Height/ {h=$2} END {print x, y, w, h}'; }
set -- $(geo "$XQ"); QX=$1; QY=$2; QW=$3; QH=$4
BINX=$((QX + 60)); BINY=$((QY + 22)); TLX=$((QX + QW - 150)); TLY=$((QY + QH / 2))
mdrag() {
    xdotool mousemove "$1" "$2"; sleep 0.4
    xdotool mousedown 1; sleep 0.4
    for s in 1 2 3 4 5 6 7 8; do xdotool mousemove $(( $1 + ($3 - $1) * s / 8 )) $(( $2 + ($4 - $2) * s / 8 )); sleep 0.12; done
    sleep 0.6; xdotool mouseup 1; sleep 1.5
}
xdotool mousemove "$BINX" "$BINY" click 1; sleep 0.8
mdrag "$BINX" "$BINY" "$TLX" "$TLY"
grep -q '^DROP internal clip-a' "$L" && pass "bin -> timeline: Qt's own drag inside the framed program drops" \
    || fail "bin -> timeline: no drop ($(grep -E 'DRAG|DROP' "$L" | tr '\n' ' '))"

# File Explorer's side: a Wine window that drags files (7787)
SLOG="$WINEPREFIX/drive_c/s.log"; : > "$SLOG"
XDNDSRC_LOG='C:\s.log' "$WINE" 'C:\xdndsrc-probe.exe' source 700 420 files "Z:$(echo "$T/files/clip-one.txt" | tr / '\\')" >/dev/null 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 150); do grep -q READY "$SLOG" 2>/dev/null && break; sleep 0.2; done
SW=$(xdotool search --name '^sg-xdnd-source$' | head -1)
if [ -n "$SW" ]; then
    set -- $(geo "$SW"); SX=$(($1 + $3 / 2)); SY=$(($2 + $4 / 2))
    mdrag "$SX" "$SY" "$TLX" "$TLY"
    grep -q "^DROP uri file://.*clip-one.txt" "$L" && pass "Wine window -> timeline: the file arrives ($(grep '^DROP uri' "$L" | tail -1 | cut -c1-80))" \
        || fail "Wine window -> timeline: nothing dropped ($(tail -2 "$SLOG" | tr '\n' ' '); $(grep -E 'DROP' "$L" | tr '\n' ' '))"
else
    fail "the Wine source window did not come up ($(cat "$SLOG"))"
fi

# the real File Explorer: its list view drags clip-two.txt onto the timeline
dprobe() { (cd "$WINEPREFIX/drive_c" && "$WINE" drag-probe.exe "$@" 2>/dev/null | tr -d '\r'); }
"$WINE" explorer "Z:$(echo "$T/clips" | tr / '\\')" >/dev/null 2>&1 &
for i in $(seq 60); do dprobe lv clips | grep -q 'clip-two.txt' && break; sleep 0.5; done
dprobe place clips 300 380 600 300 >/dev/null; dprobe view clips details >/dev/null; sleep 1.5
ITEM=$(dprobe lv clips | awk '/clip-two.txt/ {print $3; exit}')
if [ -n "$ITEM" ]; then
    FX=$(( ${ITEM%,*} + 20 )); FY=${ITEM#*,}
    mdrag "$FX" "$FY" "$TLX" "$TLY"
    if grep -q "^DROP uri file://.*clip-two.txt" "$L" && [ -f "$T/clips/clip-two.txt" ]; then
        pass "File Explorer -> timeline: the file arrives, still where it was ($(grep '^DROP uri' "$L" | tail -1 | cut -c1-80))"
    else fail "File Explorer -> timeline: $(grep -E 'DROP' "$L" | tr '\n' ' '); file there: $([ -f "$T/clips/clip-two.txt" ] && echo yes || echo no)"; fi
else
    fail "File Explorer's list did not show clip-two.txt ($(dprobe lv clips | head -3 | tr '\n' ' '))"
fi

echo
[ $RC = 0 ] && echo "xdndqt-gate: PASS" || echo "xdndqt-gate: FAIL"
exit $RC
