#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Globalization.Language statics (patches/sg/1529): IsWellFormed,
# CurrentInputMethodLanguageTag and TrySetInputMethodLanguageTag. Publisher
# asks for them as a text box takes the caret; Wine had no such interface.
#
#   WINE=/opt/wine-sg/bin/wine test/langstatics-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-langstatics.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/langstatics-probe.c" -lruntimeobject || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/langstatics-probe.c" -lruntimeobject || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    case "$O" in "statics=0 wf=1100 tag=en-US "*) pass "$a-bit: well-formed tags and the input language" ;; *) fail "$a-bit statics: $O" ;; esac
    case "$O" in *" statics2=0 tryset=0") pass "$a-bit: no layout for zz-ZZ, so it is not switched" ;; *) fail "$a-bit statics2: $O" ;; esac
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
