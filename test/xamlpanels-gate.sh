#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml panels (patches/sg/6134): Panel.Children (a UIElementCollection: Append, GetAt,
# IndexOf, RemoveAt, one parent per element), Grid with Auto, pixel and star rows and columns,
# ColumnSpacing and Grid.Row/Column, StackPanel (Spacing, Orientation) and Border (thickness and
# padding around its child). There were none. test/xamlpanels-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlpanels-gate.sh
# Mutant (dlls/windows.ui.xaml/panel.c): SG_MUTANT_GRID_NO_STARS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlpanels.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlpanels-probe.exe" "$HERE/xamlpanels-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlpanels-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlpanels-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qxF "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'getcolumn=2' "Grid.SetColumn/GetColumn (attached)"
chk 'columns=60,100,200' "columns Auto, *, 2* over 360: the Auto one fits its content, the stars share the rest"
chk 'rows=40,200' "rows 40 and *: the star takes the rest"
chk 'cells=60x30,100x200,200x200' "the children laid out in their cells"
chk 'unbounded=60x40' "with unbounded space the stars size to their content"
chk 'spacing=80,160' "ColumnSpacing takes from the stars"
chk 'children=3 getat=1 indexof=2,1 iterable=1' "Children: Size, GetAt, IndexOf, IIterable"
chk 'parent=1' "a child's Parent is its panel"
chk 'second_parent=80070057' "an element in a panel cannot be added to another"
chk 'removed=2 readd=00000000' "RemoveAt; then it can"
chk 'vertical=70x55' "StackPanel: its children one under the other, with Spacing"
chk 'horizontal=125x30' "StackPanel, Orientation Horizontal"
chk 'border=20x20 child=10x10' "Border: BorderThickness and Padding around its child"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
