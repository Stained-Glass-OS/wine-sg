#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition shapes drawn in Windows.UI.Xaml (patches/sg/6265): a ShapeVisual set as an element's
# child visual (ElementCompositionPreview) draws its sprite shapes -- a rectangle, an ellipse in a container
# shape moved by its Offset, a path over the program's own geometry source (CompositionPath over an
# IGeometrySource2DInterop); an animation paused through its AnimationController (ICompositionObject4) is at
# the progress it is given; a boolean key frame animation (ICompositor6) hides a visual. What Lottie's
# animated visuals are made of (WinUI 2's AnimatedIcon: Windows Terminal's new-tab chevron). A program's icon over
# PathIcon (AnimatedIcon is one) has a Panel as its first visual child, and its OnApplyTemplate is called: that is
# where AnimatedIcon puts its visual. The island draws again when what a child visual draws changes, not for
# every commit (an animation running on a visual drawn nowhere). test/compshapes-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/compshapes-gate.sh
# Mutants: SG_MUTANT_NO_SHAPES, SG_MUTANT_SHAPES_NOT_ANIMATED (dlls/dcomp/shapes.c), SG_MUTANT_SHAPES_NOT_DRAWN
# (dlls/dcomp/composition.c),
# SG_MUTANT_CONTROLLER_IGNORED (dlls/dcomp/animation.c), SG_MUTANT_COMP_CHILD_NOT_DRAWN (dlls/windows.ui.xaml/render.c),
# SG_MUTANT_ICON_NO_ROOT_PANEL, SG_MUTANT_ICON_NO_APPLY_TEMPLATE (dlls/windows.ui.xaml/buttons.c),
# SG_MUTANT_COMP_STAMP_IGNORED (dlls/windows.ui.xaml/render.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-compshapes.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/compshapes-probe.exe" "$HERE/compshapes-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid -ld2d1 || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/compshapes-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" compshapes-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^compositor=1,1$' "the element's compositor, ICompositor5"
chk '^shape_visual=1$' "a ShapeVisual"
chk '^path=1$' "a CompositionPath over the program's geometry source, in a path geometry"
chk '^drawn=11111$' "drawn as the element's child visual: rectangle, path, ellipse moved by its container, nothing else"
chk '^controller=1$' "the running animation's AnimationController"
chk '^progress=11$' "paused at the progress it is given: the rectangle moved down"
chk '^hidden=1,1$' "a boolean key frame animation at three quarters: the visual hidden"
chk '^icon=1,1,1,1,1$' "a program's icon over PathIcon: a Panel with a placeholder as its first child, its OnApplyTemplate when measured"
chk '^quiet=1,[0-5]$' "a visual animated where nothing draws it: the island does not draw again for it"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
