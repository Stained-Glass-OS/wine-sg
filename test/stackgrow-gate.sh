#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A stack a program manages itself grows read-write (patches/sg/0081).
#
# The Cygwin/MSYS runtime (Git Bash's mintty) runs on stacks it reserves
# no-access and commits read-write at the top, with a guard region below.
# Wine grew such a stack into committed pages with no access, and the next
# touch killed the process with SIGSEGV -- which Windows code sees as an exit
# code of 0. test/stackgrow-probe.c builds that stack, recurses 200 KB into
# it in a child, and requires the child to live and the pages to be
# read-write.
#
#   WINE=/opt/wine-sg/bin/wine test/stackgrow-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-stackgrow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O1 -o "$T/stackgrow-probe.exe" "$HERE/stackgrow-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/stackgrow-probe.exe" 2>/dev/null | tr -d '\r')
echo "      $out"
case "$out" in
    "grew=1 "*) echo "PASS  a self-managed (Cygwin-style) stack grows 200 KB into read-write pages"; echo "RESULT: PASS" ;;
    *) echo "FAIL  the stack did not grow: $out"; echo "RESULT: FAIL"; exit 1 ;;
esac
