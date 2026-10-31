#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml.Media.ImageBrush (patches/sg/6227): activatable, its statics; a Border's Background
# and an Ellipse's Fill drawn from its ImageSource by Stretch (Fill, Uniform) and AlignmentY, inside
# the area. OneDrive's last setup page needs one (it ended on REGDB_E_CLASSNOTREG). test/xamlimagebrush-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlimagebrush-gate.sh
# Mutant: SG_MUTANT_IMAGE_BRUSH_NOT_DRAWN (dlls/windows.ui.xaml/media.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlimagebrush.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlimagebrush-probe.exe" "$HERE/xamlimagebrush-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-264}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/xamlimagebrush-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
# a 4x2 opaque blue PNG
python3 - "$C/blue.png" <<'EOF2'
import struct, sys, zlib
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
raw = b''.join(b'\0' + b'\x00\x00\xff' * 4 for _ in range(2))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 2, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
EOF2
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlimagebrush-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^activate=00000000,1,1$' "ImageBrush activates, is a TileBrush, ImageSourceProperty"
chk '^loaded=1$' "markup with ImageBrushes"
chk '^fill=blue,blue$' "a Border's Background: the image, Stretch Fill"
chk '^uniform=blue,white$' "Stretch Uniform, AlignmentY Top: the image at the top, the rest empty"
chk '^ellipse=blue,white$' "an Ellipse's Fill: the image inside the ellipse only"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
