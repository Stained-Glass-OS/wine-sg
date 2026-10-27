#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml layout (patches/sg/6133): UIElement and FrameworkElement -- Measure/Arrange
# around an element's MeasureOverride/ArrangeOverride (an application's, as when it derives its own
# element), Margin, Width/Height, Min/Max, alignments, Visibility, DesiredSize/RenderSize/
# ActualWidth, InvalidateMeasure/UpdateLayout, the layout properties through the property system.
# There was no element at all. test/xamllayout-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamllayout-gate.sh
# Mutant (dlls/windows.ui.xaml/element.c): SG_MUTANT_LAYOUT_NO_MARGIN.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamllayout.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamllayout-probe.exe" "$HERE/xamllayout-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamllayout-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamllayout-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qxF "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'plain=100x50 avail=300x200 final=300x200 render=300x200 actual=300' "an element that wants 100x50, stretched over 300x200 (its own MeasureOverride/ArrangeOverride)"
chk 'margin=140x110 avail=260x140 final=260x140 render=260x140' "Margin around the element's sizes"
chk 'width=80x50 avail=80x200 render=80x50' "Width, aligned left and top: not stretched"
chk 'minmax=150x30 render=150x30' "MinWidth and MaxHeight"
chk 'collapsed=0x0 measures=0' "Collapsed: no size, not measured"
chk 'cached=1 remeasured=2' "measured once for the same space; again after InvalidateMeasure + UpdateLayout"
chk 'width_default_nan=1' "WidthProperty's default is NaN (auto)"
chk 'setvalue_width=120 desired=120 remeasured=1' "SetValue(WidthProperty) is Width, and re-measures"
chk 'own=0x0 render=300x200' "our own FrameworkElement: nothing to measure, stretched"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
