#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# icu.dll's locale list has no en_US_POSIX (wine-sg 7806), as Windows' icu.dll:
# .NET's CultureInfo.GetCultures builds a culture for every listed locale and
# on Windows "en-us-posix is an invalid culture identifier", so it threw --
# Visual Studio's ServiceHub service discovery (VSIX discovery in
# VSDetouredHost) stopped there and the IDE said "Could not start". Every
# other host locale stays, once and in order. test/icuposix-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/icuposix-gate.sh
# Mutant (dlls/icu/unixlib.c): SG_MUTANT_ICU_LISTS_POSIX -> posix=1.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icuposix.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/icuposix-probe.exe" "$HERE/icuposix-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/icuposix-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" icuposix-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=1$' "icu.dll exports uloc_countAvailable and uloc_getAvailable"
chk '^posix=0$' "no en_US_POSIX in the locale list (.NET's GetCultures throws on it on Windows)"
chk '^enus=1$' "en_US is listed, once"
chk '^count_big=1$' "and the host's other locales (more than 100)"
chk '^nulls=0$' "every index below the count names a locale"
chk '^unordered=0$' "in order, none twice (the indexes after the one left out move down)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
