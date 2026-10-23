#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# vbscript: Nothing used as an object -- x(0), x.Foo, x.Foo = 1, Set x.Bar = ..., x.Baz 1, 2 -- is
# VBScript's "Object required" (424), which On Error Resume Next can skip (6035). The call with
# arguments crashed the script host (a NULL dispatch called), the others failed with E_FAIL.
#   WINE=/opt/wine-sg/bin/wine test/vbsnothing-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vbsnothing.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/vbsnothing.vbs" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" cscript //nologo 'C:\vbsnothing.vbs' 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect call 424 "Nothing called with an argument, x(0): Object required"
expect member 424 "a property of Nothing, x.Foo"
expect assign 424 "an assignment to a property of Nothing"
expect set 424 "a Set of a property of Nothing"
expect method 424 "a method of Nothing called as a statement"
expect alive 1 "and the script goes on"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
