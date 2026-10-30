#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What React Native for Windows sets on Windows.UI.Xaml elements (patches/sg/6207): Media3D
# PerspectiveTransform3D/CompositeTransform3D and UIElement.Transform3D (kept, drawn flat);
# RectangleGeometry and UIElement.Clip (drawn: an element shows only inside its clip); the other
# UIElement statics (Projection, CacheMode, Transitions, Is*Enabled, ManipulationMode); Control's
# statics 2-7 (CornerRadiusProperty, attached IsTemplateFocusTarget); TextBlock.TextTrimming (an
# ellipsis keeps the text within its width) and TextHighlighters; UIElement.Shadow taken without
# releasing the caller's reference. OneDrive's interface stopped at each. test/xamlrnprops-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlrnprops-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_NO_TRANSFORM3D (transforms3d.c), SG_MUTANT_NO_RECTANGLE_GEOMETRY
# (geometry.c), SG_MUTANT_XAML_CLIP_IGNORED (element.c), SG_MUTANT_NO_CONTROL_STATICS7 (control.c),
# SG_MUTANT_TEXT_NO_TRIMMING, SG_MUTANT_NO_TEXT_HIGHLIGHTERS (text.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlrnprops.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlrnprops-probe.exe" "$HERE/xamlrnprops-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlrnprops-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlrnprops-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^perspective=1 depth=1000 offset=12.5 static=1$' "PerspectiveTransform3D: Depth 1000 by default, OffsetX kept, its statics"
chk '^composite3d=1 scale=1$' "CompositeTransform3D: ScaleX 1 by default"
chk '^transform3d=00000000,1$' "UIElement.Transform3D kept"
chk '^transform3d_static=1$' "UIElement.Transform3DProperty"
chk '^rect_geometry=1 rect=0,0,50x50 bounds=50x50 static=1$' "RectangleGeometry: Rect, Bounds, RectProperty"
chk '^clip=00000000,1$' "UIElement.Clip kept"
chk '^ui_statics=1111111$' "UIElement's Clip, Projection, Transitions, CacheMode, Is*Enabled, ManipulationMode statics"
chk '^tap_enabled=1 manipulation=10000$' "IsTapEnabled true and ManipulationMode System by default"
chk '^tap_disabled=1$' "IsTapEnabled set"
chk '^control_statics=1,1 focus_target=1$' "Control's statics 3 and 7: attached IsTemplateFocusTarget, CornerRadiusProperty"
chk '^trimming=1 trimmed=0,1 within=1$' "TextTrimming CharacterEllipsis cuts a long line to its width; IsTextTrimmed tells"
chk '^highlighters=1,0$' "TextBlock.TextHighlighters: an empty collection"
chk '^shadow=1,00000000,1$' "UIElement.Shadow keeps its own reference; null clears it"
chk '^clip_drawn=1,1$' "a clipped element shows only inside its clip"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
