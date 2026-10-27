#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# icu.dll (patches/sg/6127): Windows' combined ICU library -- general categories, simple case
# folding (with the Turkic option), code point counts, UTF-16 to UTF-32, converters through icuuc.
# Windows Terminal links to icu.dll and would not load without it; nor without the api set
# ext-ms-win-shell32-shellfolders-l1-1-0 (shell32), checked here too. test/icu-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/icu-gate.sh
# Mutant (dlls/icu/uchar.c): SG_MUTANT_ICU_CHARTYPE_ZERO; the api set: its line in
# dlls/apisetschema/apisetschema.spec removed -> apiset=0.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icu.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/icu-probe.exe" "$HERE/icu-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/icu-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" icu-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=1$' "icu.dll loads, with u_charType, u_foldCase, u_countChar32, u_strToUTF32"
chk '^type=1 2 9 12 6 25 27 17 18 0 20 21 19 22 24$' "general categories: Lu Ll Nd Zs Mn Sc So Co Cs Cn Ps Pe Pd Pc Sm"
chk '^fold=61 130 69 131 df 10428$' "case folding: A, dotted I (default and Turkic), I (Turkic), capital sharp s, Deseret"
chk '^count=3 2 3$' "code points: a pair counts once, an unpaired surrogate once"
chk '^utf32=0 3 61 1f600 62$' "UTF-16 to UTF-32"
chk '^utf32_small=15 3$' "a buffer too small: U_BUFFER_OVERFLOW_ERROR, and the length needed"
chk '^utf32_bad=10$' "an unpaired surrogate: U_INVALID_CHAR_FOUND"
chk '^ucnv=1$' "the converters (icuuc's) are there too"
chk '^apiset=1$' "the api set ext-ms-win-shell32-shellfolders-l1-1-0 is shell32 (SHGetKnownFolderPath)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
