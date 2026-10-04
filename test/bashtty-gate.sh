#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bash typed into from a console in a Unix terminal (patches/sg/0781): over
# ssh or in a terminal, PowerShell (here cmd) -> bash -> typed commands reach
# bash, Ctrl+C stops bash's command and not the console's programs, and after
# exit typing reaches the console's program again. conhost read the terminal
# all along and kept it raw: what was typed into bash went half to it (David
# 2026-10-03). Drives a pseudo-terminal (bashtty-drive.py).
#
#   WINE=/opt/wine-sg/bin/wine test/bashtty-gate.sh   (mutant SG_MUTANT_TTY_NOT_PAUSED)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v python3 >/dev/null && command -v bash >/dev/null || { echo "SKIP: python3 and bash needed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bashtty.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cd "$T" && timeout -s KILL 180 python3 "$HERE/bashtty-drive.py" "$WINE"
rc=$?
[ "$rc" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$rc"
