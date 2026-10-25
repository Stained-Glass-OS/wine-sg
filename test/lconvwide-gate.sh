#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcrt.dll's struct lconv has the wide members (patches/sg/6534): localeconv() gives
# _W_decimal_point and the rest after the narrow members, as the mingw headers declare for
# msvcrt.dll since Windows 7. Python 3.8 built for msvcrt.dll (OpenShot's) reads them in
# locale.localeconv(), and OpenShot crashed on Export Video reading narrow data as a pointer.
#   WINE=/opt/wine-sg/bin/wine test/lconvwide-gate.sh
# Mutant: SG_MUTANT_LCONV_NARROW (dlls/msvcrt/msvcrt.h).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-lconvwide.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/lconvwide-probe.c" || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check c_wide        "C locale: the wide members are \".\" and empty strings"
check german_narrow "German locale: the narrow members are \",\" and \".\""
check german_wide   "German locale: the wide members match (\",\", \".\", the euro sign)"
check mixed_wide    "LC_NUMERIC back to C: numeric wide members follow, monetary ones stay"
check done          "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
