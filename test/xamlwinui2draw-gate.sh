#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml drawing of WinUI 2's pieces (patches/sg/6177): a Path filled from path markup, a
# ScrollBar's thumb where its Value puts it, AcrylicBrush and XamlCompositionBrushBase in their
# FallbackColor, a BitmapIcon's picture, a ToolTip object shown as its content in the tooltip colours,
# FlyoutBase.ShowAttachedFlyout. test/xamlwinui2draw-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlwinui2draw-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_PATH_NOT_PARSED (geometry.c), SG_MUTANT_SCROLL_BAR_THUMB_FIXED
# (range.c), SG_MUTANT_ACRYLIC_NO_FALLBACK, SG_MUTANT_COMPOSITION_BRUSH_NO_FALLBACK (media.c),
# SG_MUTANT_BITMAP_ICON_NOT_SHOWN (icons.c), SG_MUTANT_TOOLTIP_NO_CONTENT (tooltip.c),
# SG_MUTANT_NO_ATTACHED_FLYOUT (flyouts.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlwinui2draw.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlwinui2draw-probe.exe" "$HERE/xamlwinui2draw-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid -lm || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlwinui2draw-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlwinui2draw-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with a Path, a ScrollBar, composition brushes, a BitmapIcon and a ToolTip"
chk '^path=1,1$' "the Path filled where its geometry is, not beyond"
chk '^scrollbar=1,1$' "the ScrollBar's thumb drawn on its track, where its Value puts it"
chk '^acrylic=1$' "an AcrylicBrush drawn in its FallbackColor"
chk '^compositionbrush=1$' "a XamlCompositionBrushBase drawn in its FallbackColor"
chk '^bitmapicon=1$' "the BitmapIcon's picture"
chk '^tooltip=1,1$' "a ToolTip object shown as its content, in the tooltip colours"
chk '^tipsame=1,1$' "a ToolTip object's tip framed as a string tooltip of its content"
chk '^attached=1,1,1$' "FlyoutBase.ShowAttachedFlyout shows the element's attached MenuFlyout"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
