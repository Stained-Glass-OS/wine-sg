#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bash.exe: the Linux side's bash from PowerShell or cmd (patches/sg/0776;
# David 2026-10-03, "a way to enter bash from PowerShell"):
#   1. it runs as this account, in this folder, and its output reaches the
#      console
#   2. its exit status is the program's (Wine gives a Unix program's
#      process none of its own)
#   3. arguments arrive as given: quotes, backslashes, spaces
#   4. cmd finds it by name (it is in system32) and sees its status
# Mutant SG_MUTANT_BASH_NO_STATUS fails it.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxbash-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxbash.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/here"
cd "$T/here" || exit 1
out=$("$WINE" bash -c 'echo "hi $(id -un) in $PWD"; exit 7' 2>/dev/null | tr -d '\r'); rc=$?
out_rc=$("$WINE" bash -c 'exit 7' >/dev/null 2>&1; echo $?)
[ "$out" = "hi $(id -un) in $T/here" ] && pass "it runs as this account, in this folder, its output in the console" || fail "output: '$out'"
[ "$out_rc" = 7 ] && pass "its exit status is the program's (7)" || fail "exit status: $out_rc"
args=$("$WINE" bash -c 'printf "[%s]" "$@"' x 'a "b" c' 'd\e' 'f\\' 2>/dev/null | tr -d '\r')
[ "$args" = '[a "b" c][d\e][f\\]' ] && pass "arguments arrive as given: $args" || fail "arguments: $args"
# a batch file, as a person types it (a cmd line passed from here through
# Wine's own argument quoting arrives with \" in it)
printf '@bash -c "exit 3" && echo status=zero || echo status=failed\r\n@bash -c "exit 0" && echo status=zero || echo status=failed\r\n' > "$T/here/t.bat"
set -- $("$WINE" cmd /c t.bat 2>/dev/null | tr -d '\r' | grep status=)
cmd=${1:-}; ok=${2:-}
[ "$cmd" = "status=failed" ] && [ "$ok" = "status=zero" ] && pass "cmd finds it by name and sees its status" || fail "cmd: '$cmd' / '$ok'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
