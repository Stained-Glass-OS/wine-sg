#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A maximized Linux program keeps its frame's size when it resizes itself
# (0893). GTK sizes its own window for a new display scale (sg-session's
# XSETTINGS manager tells it while it runs, 0.1.0-124): twice as large at
# 150%, half again at 100% -- in a maximized frame it was cut off, or a
# blank margin was left (Thunderbird, 2026-10-05). A window manager holds a
# maximized window to its size; the frame now does. Under Xvfb, an xterm
# framed as the session frames it (sg_embed, a stand-in sg-lockctl):
#   1. its frame maximized: the xterm fills it
#   2. it resizes itself to half: put back to the frame's size
#   3. restored, it resizes itself: the frame follows it (as before)
#
#   WINE=/opt/wine-sg/bin/wine test/embedscale-gate.sh
#   (mutant SG_MUTANT_EMBED_MAX_FOLLOWS in dlls/winex11.drv/sg_embed.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xterm xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedscale.XXXXXX); XP=; XT=
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

"$WINE" probe.exe maximize SgLinuxWindow "Linux Terminal" >/dev/null 2>&1; sleep 2
s1=$(size)
[ "${s1%x*}" -gt 900 ] 2>/dev/null && pass "maximized, the xterm fills its frame: $s1" || fail "maximized: the xterm $s1"
xdotool windowsize "$XID" $(( ${s1%x*} / 2 )) $(( ${s1#*x} / 2 )); sleep 2
s2=$(size)
[ "$s2" = "$s1" ] && pass "it resized itself to half (as GTK does for a new scale): put back to its maximized frame's $s2" \
    || fail "it resized itself, maximized: now $s2 (want $s1, its frame's)"
"$WINE" probe.exe restore SgLinuxWindow "Linux Terminal" >/dev/null 2>&1; sleep 2
xdotool windowsize "$XID" 600 300; sleep 2
s3=$(size); set -- $("$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r')
fw=$(( ${3:-0} - ${1:-0} )) fh=$(( ${4:-0} - ${2:-0} ))
# its frame is its size and the frame's borders and title bar
if [ $(( fw - ${s3%x*} )) -ge 0 ] && [ $(( fw - ${s3%x*} )) -le 20 ] && [ $(( fh - ${s3#*x} )) -ge 0 ] && [ $(( fh - ${s3#*x} )) -le 60 ]; then
    pass "restored, it sizes itself and its frame follows ($s3 in a ${fw}x$fh frame)"
else
    fail "restored: the xterm $s3, its frame ${fw}x$fh (want the frame around it)"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
