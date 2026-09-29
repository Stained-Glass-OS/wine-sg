#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcp's _Sinh, _Cosh and their float/long double forms (patches/sg/0509):
# Microsoft's C++ library computes std::complex's sin, cos, sinh, cosh and
# tan through them; Wine had stubs, and LibreOffice for Windows ended at
# Calc's IMSINH. Checked against the C library's sinh/cosh in msvcp140, 120,
# 110 and 100: the values, y = 0, NaN, and |x| > 709 where sinh alone
# overflows but y * sinh(x) need not.
#
#   WINE=/opt/wine-sg/bin/wine test/sinh-gate.sh
set -u
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY   # never the user's display: a stock Wine's crash report would show there
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sinh.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/sinh-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\WineDbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>"$T/err" | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
grep -q "unimplemented" "$T/err" && fail "a stub was called: $(grep -m1 unimplemented "$T/err")"
for d in msvcp140 msvcp120 msvcp110 msvcp100; do
    [ "$(sed -n "s/^$d.dll values //p" "$T/out")" = "1 1 1 1 1 1" ] \
        && pass "$d: _Sinh, _Cosh, _LSinh, _LCosh, _FSinh, _FCosh give y*sinh(x), y*cosh(x)" || fail "$d values: $(grep "^$d.dll" "$T/out")"
    [ "$(sed -n "s/^$d.dll edges //p" "$T/out")" = "1 1 1 1" ] \
        && pass "$d: y = 0, NaN, and |x| > 709 without overflowing" || fail "$d edges"
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
