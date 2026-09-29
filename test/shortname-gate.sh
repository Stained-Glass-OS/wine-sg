#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SetFileShortName (patches/sg/0494). A helper Word starts called
# SetFileShortNameW, which kernel32 lacked, and aborted. The file system here
# derives 8.3 names from long ones and keeps none of its own, so a valid 8.3
# name for an open file is taken, to no effect; names that are not 8.3
# (too long, a bad character, two dots, nothing before the dot, empty) are
# refused with ERROR_INVALID_PARAMETER, and a handle that is not a file's
# with ERROR_INVALID_HANDLE. SetFileShortNameA as well.
#
#   WINE=/opt/wine-sg/bin/wine test/shortname-gate.sh
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

T=$(mktemp -d /var/tmp/sg-shortname.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'LIBRARY kernel32.dll\nEXPORTS\nSetFileShortNameW\nSetFileShortNameA\n' > "$T/k.def"
"${DLLTOOL:-x86_64-w64-mingw32-dlltool}" -d "$T/k.def" -l "$T/libshort.a" || { fail "no import library"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/shortname-probe.c" "$T/libshort.a" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v valid)" = "1 0" ] && [ "$(v noext)" = "1 0" ] && pass "a valid 8.3 name is taken (with and without an extension)" || fail "valid: $(v valid) / $(v noext)"
bad=""; for n in toolong longext star twodots dotfirst empty; do [ "$(v $n)" = "0 87" ] || bad="$bad $n=$(v $n)"; done
[ -z "$bad" ] && pass "names that are not 8.3 are refused with ERROR_INVALID_PARAMETER" || fail "not refused:$bad"
[ "$(v badhandle)" = "0 6" ] && pass "a handle that is not a file's: ERROR_INVALID_HANDLE" || fail "bad handle: $(v badhandle)"
[ "$(v ansi)" = "1 0" ] && pass "SetFileShortNameA" || fail "ansi: $(v ansi)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
