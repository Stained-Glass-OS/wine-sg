#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The UCRT gives a program whose manifest asks for UTF-8 as its code page (activeCodePage,
# Windows 10 1903) UTF-8 locales, as Windows' does (wine-sg 8406): setlocale(LC_ALL, "") is
# "<language>_<country>.utf8" with code page 65001. Wine took the locale's own ANSI code page
# (1252), and R 4.2+ -- which relies on it -- started with "Using locale code page other than
# 65001 (UTF-8) may cause problems" and ran its console in 1252. An explicit code page still wins;
# a program without the manifest is unchanged.
# test/ucrtutf8-probe.c built with test/ucrtutf8.manifest (RT_MANIFEST 1) and without, 64/32-bit.
#   WINE=/opt/wine-sg/bin/wine test/ucrtutf8-gate.sh
# Mutant: -DSG_MUTANT_UCRT_LOCALE_IGNORES_UTF8_ACP (dlls/msvcrt/locale.c, ucrtbase).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc x86_64-w64-mingw32-windres i686-w64-mingw32-windres; do
    command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ucrtutf8.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf '1 24 "%s"\n' "$HERE/ucrtutf8.manifest" > "$T/m.rc"
for a in x86_64 i686; do
    $a-w64-mingw32-windres "$T/m.rc" -O coff -o "$T/m-$a.o" || { fail "$a manifest did not build"; exit 1; }
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/utf8-$a.exe" "$HERE/ucrtutf8-probe.c" "$T/m-$a.o" || { fail "$a probe did not build"; exit 1; }
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/plain-$a.exe" "$HERE/ucrtutf8-probe.c" || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/utf8-$a.exe" "$T/plain-$a.exe" "$WINEPREFIX/drive_c/"
    u=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\utf8-$a.exe" 2>/dev/null | tr -d '\r')
    p=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\plain-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$u" "$p" | sed "s/^/      $a: /"
    case "$u" in *"acp=65001 cp=65001 name="*".utf8"*) pass "$a: activeCodePage UTF-8: setlocale(LC_ALL, \"\") is <locale>.utf8, code page 65001" ;;
        *) fail "$a: UTF-8 manifest: '$(printf '%s' "$u" | head -1)'" ;; esac
    printf '%s\n' "$u" | grep -q '^explicit=1252$' && pass "$a: an explicit .1252 still wins" || fail "$a: explicit: '$u'"
    acp=$(printf '%s\n' "$p" | sed -n 's/^acp=\([0-9]*\) .*/\1/p')
    [ -n "$acp" ] && [ "$acp" != 65001 ] && printf '%s\n' "$p" | grep -q "^acp=$acp cp=$acp name=.*\.$acp\$" \
        && pass "$a: without the manifest: the system's code page ($acp), as before" || fail "$a: no manifest: '$p'"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
