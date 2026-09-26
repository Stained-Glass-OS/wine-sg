#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0249: a wineserver started on its own raises its open-files
# limit, as Wine's clients raise theirs.
#
# The shared prefix's machine wineserver is started by systemd with the soft
# limit of 1024 descriptors, and it holds one for every file, mapping and
# socket of every process attached to it: one Firefox exhausted it and every
# further DLL load failed with STATUS_TOO_MANY_OPENED_FILES ("Couldn't load
# XPCOM", tabs "Exiting due to channel error"). Here the server is started as
# systemd would, with a soft limit of 1024, and a program opens 1500 files.
#   WINE=... test/serverfds-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
N=${N:-1500}
W=$(mktemp -d /var/tmp/serverfds.XXXXXX)
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
command -v "$MINGW" >/dev/null && command -v prlimit >/dev/null || { echo "SKIP: no mingw/prlimit"; exit 77; }
cleanup() { set +e; "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/serverfds-probe.c" || { echo "FAIL build"; exit 1; }
"$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
hard=$(ulimit -Hn)
[ "$hard" = unlimited ] && hard=1048576
[ "$hard" -gt $((N + 200)) ] 2>/dev/null || { echo "SKIP: hard open-files limit $hard is too low to tell"; exit 77; }
# as systemd starts the machine server: soft 1024, hard as allowed
prlimit --nofile=1024:"$hard" "$WINESERVER" -p
sleep 1
pid=
for p in $(pgrep -u "$(id -u)" -x wineserver); do     # this prefix's server, not another one's
    tr '\0' '\n' < "/proc/$p/environ" 2>/dev/null | grep -qx "WINEPREFIX=$WINEPREFIX" && pid=$p
done
[ -n "$pid" ] || { echo "FAIL no wineserver for $WINEPREFIX"; exit 1; }
soft=$(awk '/Max open files/ {print $4}' "/proc/$pid/limits")
[ "$soft" = "$hard" ] && pass "the server raised its open-files limit to $hard (started with 1024)" \
    || fail "the server's open-files limit stayed $soft (hard $hard)"
mkdir "$W/files"
out=$("$WINE" "$W/probe.exe" "$N" "Z:${W//\//\\}\\files" 2>/dev/null | tr -d '\r')
echo "  $out"
[ "$out" = "OPENED $N ERR 0" ] && pass "a program holds $N files open at once" || fail "a program could hold only: $out"
echo "serverfds-gate: $fails failure(s)"
exit $((fails > 0))
