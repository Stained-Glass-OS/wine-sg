#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcp's std::_Xregex_error (patches/sg/1520). The regex code compiled into
# programs calls it when a pattern does not parse or a match grows too
# complex; Windows throws std::regex_error (a runtime_error that keeps the
# error code). It was a stub: Outlook died with "unimplemented function
# msvcp140.dll._Xregex_error" showing an HTML mail. The probe calls it in
# msvcp140 (64- and 32-bit) and msvcp120 and inspects the exception raised.
#
#   WINE=/opt/wine-sg/bin/wine test/xregex-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null \
    || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xregex.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/xregex-probe.c" || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/xregex-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
run() { timeout 60 "$WINE" "$T/$1" $2 ${3:-} 2>/dev/null | tr -d '\r'; }
O=$(run p64.exe 4); echo "      64: $O"
case "$O" in "type=1 base=1 code=4 what=regex_error(error_brack)"*) pass "msvcp140 (64-bit) throws regex_error(error_brack) with its code" ;;
    *) fail "msvcp140 64-bit: $O" ;; esac
O=$(run p64.exe 12); echo "      64: $O"
case "$O" in "type=1 base=1 code=12 what=regex_error(error_stack)"*) pass "error_stack keeps its code and message" ;;
    *) fail "msvcp140 64-bit error_stack: $O" ;; esac
O=$(run p32.exe 1); echo "      32: $O"
case "$O" in "type=1 base=1 code=1 what=regex_error(error_ctype)"*) pass "msvcp140 (32-bit) throws regex_error(error_ctype)" ;;
    *) fail "msvcp140 32-bit: $O" ;; esac
O=$(run p64.exe 5 msvcp120.dll); echo "      120: $O"
case "$O" in "type=1 base=1 code=5 what=regex_error(error_paren)"*) pass "msvcp120 throws it too" ;;
    *) fail "msvcp120: $O" ;; esac
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
