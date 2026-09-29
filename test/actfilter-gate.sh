#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A process's activation filter (patches/sg/0493). Word registers one with
# CoRegisterActivationFilter as it starts, and aborted on the missing
# function (0xc06d007f, a delay-loaded import). The filter is asked about
# every class activation: it may name another class in its place (here
# ShellLink -> DOMDocument60, created for IXMLDOMDocument), refuse one
# (E_ACCESSDENIED), or name none, which leaves the class asked for. An
# activation the filter makes itself is not filtered again, and a process
# has one filter: a second registration is refused.
#
#   WINE=/opt/wine-sg/bin/wine test/actfilter-gate.sh
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

T=$(mktemp -d /var/tmp/sg-actfilter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'LIBRARY ole32.dll\nEXPORTS\nCoRegisterActivationFilter\n' > "$T/f.def"
"${DLLTOOL:-x86_64-w64-mingw32-dlltool}" -d "$T/f.def" -l "$T/libfilter.a" || { fail "no import library"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/actfilter-probe.c" "$T/libfilter.a" -lole32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v register)" = 00000000 ] && pass "CoRegisterActivationFilter takes a filter" || fail "register: $(v register)"
[ "$(v register-again)" = 80070005 ] && pass "and refuses a second one" || fail "second filter: $(v register-again)"
[ "$(v replaced)" = 00000000 ] && pass "the filter's replacement class is created (ShellLink -> DOMDocument60)" || fail "replacement: $(v replaced)"
[ "$(v refused)" = 80070005 ] && pass "a class the filter refuses is refused (E_ACCESSDENIED)" || fail "refused: $(v refused)"
[ "$(v unchanged)" = 00000000 ] && pass "no class named in reply: the class asked for" || fail "unchanged: $(v unchanged)"
[ "$(v inner)" = 0 ] && pass "the filter's own activation is not filtered again" || fail "inner calls: $(v inner)"
[ "$(v calls)" = 3 ] && pass "asked once for each activation (3)" || fail "calls: $(v calls)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
