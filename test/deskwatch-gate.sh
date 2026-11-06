#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop's folder watcher does not stack its requests (patches/sg/1476).
# Explorer watches the Desktop and the Public Desktop folders; after either
# changed it asked both again, so the other folder's request stacked up -- one
# more per change, each holding a buffer in explorer and in the server, never
# freed. A download into the Desktop folder writes it thousands of times: a
# session's explorer had grown to 1.3 GB (its glibc arenas full of
# FILE_NOTIFY records), its wineserver to 100 MB. test/deskwatch-probe.c
# writes a file in the Desktop folder 4000 times; explorer's anonymous memory
# and the server's must stay about where they were, and the desktop must still
# see a new file afterwards.
#
#   WINE=/opt/wine-sg/bin/wine test/deskwatch-gate.sh
# Mutant: SG_MUTANT_DESKWATCH_STACK (programs/explorer/desktop.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deskwatch.XXXXXX)
unset WAYLAND_DISPLAY
mkdir -p "$T/xdg"; chmod 700 "$T/xdg"; export XDG_RUNTIME_DIR="$T/xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/deskwatch-probe.c" -lshell32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
# the Desktop folder exists before explorer starts watching it
timeout 120 "$WINE" "$T/probe.exe" 1 >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
i=0; while [ $i -lt 60 ]; do
    EP=$(ps -eo pid,args | awk '$2 ~ /explorer\.exe$/ && $3 ~ /desktop=shell/ {print $1; exit}')
    [ -n "$EP" ] && break; sleep 0.5; i=$((i + 1))
done
SP=$(ps -eo pid,args | awk -v s="$WINESERVER" '$2 == s || $2 ~ /\/wineserver$/ {print $1; exit}')
[ -n "$EP" ] && [ -n "$SP" ] || { fail "no explorer ($EP) or server ($SP)"; exit 1; }
sleep 4
anon() { awk '/^Pss_Anon:/{print $2}' "/proc/$1/smaps_rollup" 2>/dev/null; }
timeout 120 "$WINE" "$T/probe.exe" 200 >/dev/null 2>&1; sleep 2   # warm-up
e0=$(anon "$EP"); s0=$(anon "$SP")
out=$(timeout 300 "$WINE" "$T/probe.exe" 4000 2>/dev/null | tr -d '\r'); sleep 3
e1=$(anon "$EP"); s1=$(anon "$SP")
echo "      explorer anon ${e0} -> ${e1} kB, server anon ${s0} -> ${s1} kB ($out)"
[ "$out" = "done 4000" ] && pass "4000 writes in the Desktop folder" || fail "the probe: '$out'"
[ -n "$e1" ] && [ $((e1 - e0)) -lt 4096 ] && pass "explorer's memory stays where it was (+$((e1 - e0)) kB)" \
    || fail "explorer grew by $((e1 - e0)) kB over 4000 changes"
[ -n "$s1" ] && [ $((s1 - s0)) -lt 4096 ] && pass "and the server's (+$((s1 - s0)) kB)" \
    || fail "the server grew by $((s1 - s0)) kB over 4000 changes"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
