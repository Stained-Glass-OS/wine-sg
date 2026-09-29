#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# XML Schema patterns with MSXML's \uXXXX escapes (patches/sg/0489). Office's
# installer loads App-V's manifest schema, whose patterns say
# "[^\uFDD0-\uFDEF\uFFF9-\uFFFF\p{IsPrivateUse}]+"; libxml2 knows no \u
# escape, the schema failed to parse and adding it to a schema cache failed
# with E_FAIL -- Office's error 30175-45 (xmlutils.cpp:624). The schema is
# added, and validates by the escapes' characters: capitals are refused by
# [^\u0041-\u005A]+, a space by the quote-and-space class, \u002E is a
# literal dot, and an escaped backslash before "u0041" stays a backslash.
#
#   WINE=/opt/wine-sg/bin/wine test/xsdpattern-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-xsdpattern.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/xsdpattern-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
P() { "$WINE" "$T/probe.exe" "$@" 2>/dev/null | tr -d '\r'; }

[ "$(P 0 abc)" = "add 00000000 valid 00000000" ] && pass "a pattern without escapes: added, and validates" || fail "plain: $(P 0 abc)"
r=$(P 2 abc); [ "${r%% valid*}" = "add 00000000" ] && pass "App-V's pattern (\\uFDD0-\\uFDEF\\uFFF9-\\uFFFF\\p{IsPrivateUse}) is added: $r" || fail "App-V's pattern: $r"
[ "$(P 1 abc)" = "add 00000000 valid 00000000" ] && pass "[^\\u0041-\\u005A]+ takes lower case" || fail "\\u0041-\\u005A abc: $(P 1 abc)"
r=$(P 1 aBc); [ "${r%% valid*}" = "add 00000000" ] && [ "${r##* }" != 00000000 ] && pass "and refuses a capital ($r)" || fail "\\u0041-\\u005A aBc: $r"
r=$(P 3 'a b'); [ "${r##* }" != 00000000 ] && [ "$(P 3 ab)" = "add 00000000 valid 00000000" ] && pass "the quote-and-space class (\\u0000-\\u0020) refuses a space, takes ab" || fail "space class: $r / $(P 3 ab)"
r=$(P 4 axb); [ "$(P 4 a.b)" = "add 00000000 valid 00000000" ] && [ "${r##* }" != 00000000 ] && pass "\\u002E is a literal dot, not any character" || fail "dot: $(P 4 a.b) / $r"
r=$(P 5 BSU); [ "$r" = "add 00000000 valid 00000000" ] && pass "an escaped backslash before u0041 stays a backslash" || fail "escaped backslash: $r"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
