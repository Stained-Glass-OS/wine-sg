#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GdipSetImageAttributesOutputChannelColorProfile (patches/sg/8222): with a CMYK
# profile (ghostscript's default_cmyk.icc) the output channel separates colours
# through it; a bare name is one in the colour directory. test/gdipprofile-probe.c,
# 64- and 32-bit. It returned NotImplemented.
#
#   WINE=/opt/wine-sg/bin/wine test/gdipprofile-gate.sh
# Mutant: SG_MUTANT_GDIP_NO_CHANNEL_PROFILE (gdiplus/imageattributes.c, graphics.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
ICC="${SG_CMYK_ICC:-/usr/share/color/icc/ghostscript/default_cmyk.icc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$ICC" ] || { echo "SKIP: no CMYK profile at $ICC (ghostscript)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-gdipprofile.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/gdipprofile-probe.c" -lmscms &&
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/gdipprofile-probe.c" -lmscms ||
    { fail "the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$ICC" "$WINEPREFIX/drive_c/cmyk.icc"
# the colour directory (GetColorDirectory), which a fresh prefix lacks
mkdir -p "$WINEPREFIX/drive_c/windows/system32/spool/drivers/color" "$WINEPREFIX/drive_c/windows/syswow64/spool/drivers/color"
for b in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$b.exe" 'C:\cmyk.icc' 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -q '^done=1' || { fail "$b: the probe did not finish"; continue; }
    for k in set_profile through_profile profile_differs bare_name profile_off null_name null_attr; do
        printf '%s\n' "$out" | grep -q "^$k=1" && pass "$b: $k" || fail "$b: $k ($(printf '%s\n' "$out" | grep "^$k=" || echo none))"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
