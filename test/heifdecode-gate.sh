#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HEIF/HEIC images through WIC (patches/sg/6539): windowscodecs has a HEIF decoder that loads libheif
# (and its HEVC plugin, libheif-plugin-libde265) at run time, registered with the HEIF container
# format and the .heic/.heif/.hif extensions, so that programs that decode through WIC (FastStone,
# IrfanView's WIC path) and the shell's thumbnails show iPhone photos; without libheif it declines
# and WIC finds no component (WINCODEC_ERR_COMPONENTNOTFOUND). The probe decodes a 64x64 HEIC with
# four coloured quadrants from a file and from a stream, and asks the shell for its thumbnail, as a
# 64-bit and as a 32-bit program (test/heifdecode-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/heifdecode-gate.sh
# Needs libheif1 and libheif-plugin-libde265 (SKIP without them).
# Mutants: SG_MUTANT_HEIF_NOT_REGISTERED (dlls/windowscodecs/regsvr.c),
#          SG_MUTANT_HEIF_NO_LIBRARY (dlls/windowscodecs/unixlib.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ls /usr/lib/*/libheif.so.1 >/dev/null 2>&1 || { echo "SKIP: no libheif1"; exit 77; }
ls /usr/lib/*/libheif/plugins/*de265* >/dev/null 2>&1 || { echo "SKIP: no libheif-plugin-libde265"; exit 77; }
T=$(mktemp -d /var/tmp/sg-heifdecode.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/heifdecode-probe.c" -lole32 -lwindowscodecs -lshlwapi -lshell32 -lgdi32 -luuid \
        || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check registered     "the HEIF decoder is a registered WIC component, with .heic among its extensions"
    check container_heif "a .heic file decodes, as the HEIF container format"
    check file_pixels    "64x64, 32bpp BGR, the four quadrants' colours (from the file)"
    check stream_pixels  "the same from a stream"
    check thumbnail      "the shell makes its thumbnail"
    check done           "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
