#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Graphics.Imaging.SoftwareBitmap (patches/sg/6148): created, locked through BitmapBuffer
# and IMemoryBufferByteAccess, converted between formats and alpha modes, read-only views sharing its
# pixels, CopyToBuffer, made from a WIC bitmap and from an icon (ISoftwareBitmapNativeFactory -- how
# NanaZip makes its address bar icon) and back to WIC (ISoftwareBitmapNative); bad sizes, formats
# and closed bitmaps fail. There was no Windows.Graphics.Imaging. test/imgbitmap-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/imgbitmap-gate.sh
# Mutant (dlls/windows.graphics.imaging/bitmap.c): SG_MUTANT_BITMAP_NO_BUFFER.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-imgbitmap.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/imgbitmap-probe.exe" "$HERE/imgbitmap-probe.c" -lruntimeobject -luser32 -lgdi32 -lwindowscodecs -lole32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/imgbitmap-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" imgbitmap-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^factory=00000000$' "Windows.Graphics.Imaging.SoftwareBitmap's factory"
chk '^statics=1 native_factory=1$' "its statics and ISoftwareBitmapNativeFactory"
chk '^create=00000000 format=87 alpha=0 size=4x3$' "Create(Bgra8, 4, 3): premultiplied"
chk '^lock=1 capacity=48 stride=16$' "LockBuffer: one plane, its bytes"
chk '^convert_rgba=30,20,10,255$' "Convert to Rgba8"
chk '^convert_straight=128,128$' "ConvertWithAlpha: premultiplied to straight"
chk '^view=1 write=80070005 shared=1$' "a read-only view: no writing, the same pixels"
chk '^copy_to_buffer=00000000 length=48 bytes=1$' "CopyToBuffer"
chk '^from_wic=00000000 format=87 alpha=1 readonly=1 pixel=1,2,3,200$' "from a WIC bitmap (straight BGRA), read-only as asked"
chk '^to_wic=00000000 pixel=1,2,3,200$' "back to WIC (ISoftwareBitmapNative.GetData)"
chk '^icon=16x16 pixel=0,0,255,255$' "an icon through WIC to premultiplied Bgra8 (NanaZip's address bar)"
chk '^bad_size=80070057$' "a negative size fails"
chk '^huge=1$' "a huge size fails"
chk '^bad_format=1$' "a format we do not hold fails"
chk '^gray_alpha=1$' "gray with alpha fails"
chk '^copy_mismatch=1$' "CopyTo a bitmap of another size fails"
chk '^closed=1$' "a closed bitmap fails"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
