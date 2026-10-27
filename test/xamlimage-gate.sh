#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml images (patches/sg/6151): Image with a SoftwareBitmapSource (SetBitmapAsync of a
# SoftwareBitmap, as NanaZip's address bar icon) and with a BitmapImage from markup (an ms-appx:///
# PNG decoded with WIC), Stretch Fill and Uniform, drawn in a XAML Island; a source that is not a
# SoftwareBitmap fails, a missing file draws nothing. test/xamlimage-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlimage-gate.sh
# Mutant (dlls/windows.ui.xaml/imaging.c): SG_MUTANT_IMAGE_NO_PIXELS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlimage.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlimage-probe.exe" "$HERE/xamlimage-probe.c" -lruntimeobject -luser32 -lgdi32 -lole32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlimage-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
# a 4x2 opaque blue PNG
python3 - "$WINEPREFIX/drive_c/blue.png" <<'EOF'
import struct, sys, zlib
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
raw = b''.join(b'\0' + b'\x00\x00\xff' * 4 for _ in range(2))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 2, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
EOF
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlimage-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with Images"
chk '^set_bitmap=00000000 completed=1,1$' "SoftwareBitmapSource.SetBitmapAsync completes"
chk '^not_a_bitmap=1$' "a source that is not a SoftwareBitmap fails"
chk '^png_width=4$' "BitmapImage from Source=\"ms-appx:///blue.png\": its pixel width"
chk '^software_image=1,1$' "the SoftwareBitmap drawn, Stretch Fill"
chk '^png_uniform=1,1$' "the PNG drawn, Stretch Uniform (letterboxed)"
chk '^missing=1$' "a missing file draws nothing"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
