#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program that sizes its own window keeps the width it chose (1100).
# Its frame follows the program (WM_X11DRV_SG_EMBED_SIZE), whose sizes are
# packed width | height << 16; the width was taken as the whole packed
# value, so the frame was made as wide as Wine allows -- wider than the
# screen -- and the program's window stretched to it: LXTerminal and SG
# Office 2750 px wide on a 2736x1824 screen after Settings went from 175% to
# 125% (s14 regression walk, 2026-10-06), a dialog fitting its contents as
# wide as the screen. Under Xvfb, an xterm framed as the session frames it
# (sg_embed, a stand-in sg-lockctl), restored:
#   1. it sizes itself 600x300: it stays 600 wide, its frame round it
#   2. it sizes itself 420x380 (narrower, taller): the same
#
#   WINE=/opt/wine-sg/bin/wine test/embedselfsize-gate.sh
#   (mutant SG_MUTANT_EMBED_SIZE_WIDTH in dlls/winex11.drv/sg_embed.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xterm xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedselfsize.XXXXXX); XP=; XT=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
xterm -geometry 50x12+120+120 -title 'Linux Terminal' & XT=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 6
cd "$WINEPREFIX/drive_c"
size() { xwininfo -id "$XID" 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}'; }

check() {   # W H: the xterm sized itself WxH
    xdotool windowsize "$XID" "$1" "$2"; sleep 2
    s=$(size); set -- "$1" "$2" $("$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r')
    fw=$(( ${5:-0} - ${3:-0} )) fh=$(( ${6:-0} - ${4:-0} ))
    if [ "${s%x*}" = "$1" ] && [ $(( fw - $1 )) -ge 0 ] && [ $(( fw - $1 )) -le 20 ] && [ $(( fh - ${s#*x} )) -ge 0 ] && [ $(( fh - ${s#*x} )) -le 60 ]; then
        pass "it sized itself $1x$2: it is $s, in a ${fw}x$fh frame round it"
    else
        fail "it sized itself $1x$2: it is $s, its frame ${fw}x$fh (want $1 wide, the frame round it)"
    fi
}
check 600 300
check 420 380
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
