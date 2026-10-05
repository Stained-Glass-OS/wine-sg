#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A GTK window that draws its own title bar and shadow (_GTK_FRAME_EXTENTS,
# wine-sg 0813), maximized and restored in its Wine frame (patches/sg/0842):
#   1. maximized, the frame fills the work area and stays so: it and the
#      program's window no longer see-saw between two sizes -- the frame took
#      a size of the program's window from before its own last request (its
#      shadow dropped) for the program's own and was sized to it, the
#      program's window to the frame, and so on (GNOME's File Roller in a
#      session that lists _GTK_FRAME_EXTENTS, ~60 resizes a second)
#   2. the program's window then has the frame's size, at 0,0 in it
#   3. restored, the frame is back at its size from before
# Maximized and restored as the program's own buttons ask (_NET_WM_STATE to
# the root, test/csdmaxloop-ask.c).
#
#   WINE=/opt/wine-sg/bin/wine test/csdmaxloop-gate.sh   (mutant SG_MUTANT_EMBED_SIZE_LOOP)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY
for t in Xvfb xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-csdmaxloop.XXXXXX); XP=; XC=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/csdframe-client" "$HERE/csdframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
cc -O2 -o "$T/ask" "$HERE/csdmaxloop-ask.c" -lX11 || { echo "FAIL  helper did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
case "$DISPLAY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run" SG_FAKE_XWLOG="$T/xwlog"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
cd "$WINEPREFIX/drive_c"

"$T/csdframe-client" "Csd Max" 200 150 400 300 20 20 15 25 & XC=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Csd Max"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Csd Max"/ {print $1; exit}')
printf '%d shown - CsdMax\tCsd Max\nEND\n' "$XID" > "$T/list"
framed=; i=0
while [ $i -lt 120 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
    case "$p" in ""|"(the root window)"*) ;; *) framed=1; break ;; esac
    sleep 0.05; i=$((i + 1))
done
[ -n "$framed" ] && pass "the program's window is framed ($p)" || { fail "never framed"; echo "RESULT: FAIL"; exit 1; }
sleep 1.5
rect() { "$WINE" probe.exe rect SgLinuxWindow "Csd Max" 2>/dev/null | tr -d '\r'; }
child() { xwininfo -id "$XID" | awk '/Relative upper-left X/ {x=$NF} /Relative upper-left Y/ {y=$NF} /Width:/ {w=$NF} /Height:/ {h=$NF} END {print x, y, w, h}'; }
r0=$(rect)

# as its own maximize button asks (GTK's): the program drops its shadow as
# soon as it is told it is maximized, before its frame is
"$T/ask" "$XID" 1; sleep 2
# the frame and the program's window, sampled for two seconds: one size
seen=""; cs=""
for k in 1 2 3 4 5 6 7 8; do
    seen="$seen
$(rect)"; cs="$cs
$(child)"; sleep 0.25
done
n=$(printf '%s\n' "$seen" | sed '/^$/d' | sort -u | wc -l)
c=$(printf '%s\n' "$cs" | sed '/^$/d' | sort -u | wc -l)
rm=$(printf '%s\n' "$seen" | sed '/^$/d' | tail -1)
[ "$n" = 1 ] && [ "$c" = 1 ] && pass "maximized, the frame and the program's window keep one size ($rm)" \
    || fail "maximized, they see-saw: frame $(printf '%s\n' "$seen" | sed '/^$/d' | sort -u | tr '\n' '|') program $(printf '%s\n' "$cs" | sed '/^$/d' | sort -u | tr '\n' '|')"
set -- $rm
[ $# = 4 ] && [ "$1" = 0 ] && [ "$2" = 0 ] && [ "$3" = 1024 ] && [ "$4" -ge 600 ] && [ "$4" -le 700 ] \
    && pass "maximized, the frame fills the work area ($rm)" || fail "maximized frame: $rm (want 0 0 1024 <=700)"
ch=$(child); set -- $ch
fw=$(( $(echo "$rm" | cut -d' ' -f3) - $(echo "$rm" | cut -d' ' -f1) ))
fh=$(( $(echo "$rm" | cut -d' ' -f4) - $(echo "$rm" | cut -d' ' -f2) ))
[ "$1" = 0 ] && [ "$2" = 0 ] && [ "$3" = "$fw" ] && [ "$4" = "$fh" ] \
    && pass "the program's window fills the frame at 0,0 ($ch)" || fail "the program's window: $ch (frame ${fw}x$fh)"

"$T/ask" "$XID" 0; sleep 2
r1=$(rect)
[ "$r1" = "$r0" ] && pass "restored, the frame is back at its size and place ($r1)" || fail "restored: $r1 (was $r0)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
