#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml transforms (patches/sg/6184): TranslateTransform, ScaleTransform about a centre,
# RotateTransform, CompositeTransform and its Inverse, MatrixTransform, a TransformGroup's children and
# Value, each as a GeneralTransform; an element's RenderTransform from markup, drawn moved and scaled
# from where it was laid out (Windows Terminal animates its tabs and title bar so). test/xamltransforms-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamltransforms-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_TRANSFORM_MATRIX_IDENTITY (transforms.c),
# SG_MUTANT_RENDER_TRANSFORM_IGNORED (render.c), SG_MUTANT_TRANSFORM_GROUP_SET (markup.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamltransforms.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamltransforms-probe.exe" "$HERE/xamltransforms-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamltransforms-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamltransforms-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^translate=15,2$' "TranslateTransform"
chk '^scale=14,13$' "ScaleTransform about its centre"
chk '^rotate=-2,5$' "RotateTransform by 90 degrees"
chk '^composite=20,6$' "CompositeTransform: scaled, then moved"
chk '^inverse=5,5$' "its Inverse"
chk '^matrix=5,9$' "MatrixTransform"
chk '^group=20,10 value=2,0,0,2,10,0$' "TransformGroup: its children in order; Value"
chk '^load=00000000$' "markup with RenderTransform"
chk '^render_transform=1$' "UIElement.RenderTransform from markup"
chk '^moved=1,1$' "drawn moved by its TranslateTransform"
chk '^scaled=1,1,1$' "drawn scaled, then moved, by its TransformGroup"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
