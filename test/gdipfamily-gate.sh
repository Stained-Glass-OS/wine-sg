#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GDI+ finds a font family by a name Wine maps to another font (Fonts
# \Replacements, as the system's Tahoma, Microsoft Sans Serif and Segoe UI ->
# Inter, Calibri -> Carlito) when that font's full name carries its style
# ("Inter Regular"), patches/sg/0768: Wine compared the full name with the
# families' names, "FontFamilyNotFound", and .NET programs failed in their type
# initializers (AmbirScan's GdPicture, David 2026-10-02).
#
#   WINE=/opt/wine-sg/bin/wine test/gdipfamily-gate.sh
# Mutation: build with -DSG_MUTANT_GDIP_FULLNAME_ONLY: the alias is not found.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
command -v fc-list >/dev/null || { echo "SKIP: no fontconfig"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
# a TrueType font here whose full name is its family and " Regular"
FAM=$(fc-list -f '%{family[0]}|%{fullname[0]}|%{file}\n' : family fullname file | awk -F'|' '$2 == $1 " Regular" && $3 ~ /\.ttf$/ { print $1; exit }')
[ -n "$FAM" ] || { echo "SKIP: no font named '<family> Regular' here"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gdipfamily.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -municode -O2 -o "$T/probe.exe" "$HERE/gdipfamily-probe.c" -lgdiplus || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Fonts\Replacements' /v 'SG Alias Face' /d "$FAM" /f >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$T/probe.exe" "$FAM" 2>/dev/null | tr -d '\r')
[ "$out" = "status 0 family $FAM" ] && pass "$FAM itself: $out" || fail "$FAM itself: $out"
out=$("$WINE" "$T/probe.exe" 'SG Alias Face' 2>/dev/null | tr -d '\r')
[ "$out" = "status 0 family $FAM" ] && pass "a name mapped to it (full name \"$FAM Regular\") is found: $out" \
    || fail "the mapped name: $out (want status 0 family $FAM)"
out=$("$WINE" "$T/probe.exe" 'SG No Such Face' 2>/dev/null | tr -d '\r')
[ "$out" = "status 14 family " ] && pass "and a name of nothing is not (FontFamilyNotFound)" || fail "unknown name: '$out'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
