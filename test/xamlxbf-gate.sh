#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml compiled markup (patches/sg/6139): Application.LoadComponent of compiled XAML
# (XBF v2; our fixtures, test/mkxbf.py) -- framework types and properties, enums, thicknesses,
# colours, grid lengths, attached Grid.Row, collections, x:Name, x:ConnectionId -- drawn in a XAML
# Island; broken files (truncated, a bad string index, a section past the end, an unknown node) fail
# without harm. Applications ship their pages compiled. test/xamlxbf-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlxbf-gate.sh
# Mutant (dlls/windows.ui.xaml/xbf.c): SG_MUTANT_XBF_NO_BOUNDS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlxbf.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlxbf-probe.exe" "$HERE/xamlxbf-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlxbf-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlxbf-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "LoadComponent of a compiled Grid onto a Grid"
chk '^findname=1$' "x:Name (FindName)"
chk '^rowdefinitions=2$' "a collection property (RowDefinitions)"
chk '^truncated=1$' "a truncated file fails"
chk '^bad_string=1$' "a string index past the table fails"
chk '^bad_section=1$' "a node section past the end fails"
chk '^bad_node=1$' "an unknown node fails"
chk '^row0=40$' "a pixel row height from a compiled GridLength"
chk '^background=1$' "a compiled colour drawn"
chk '^border=1$' "the Border where its compiled Grid.Row, Margin, enums and size put it"
chk '^text=1$' "the TextBlock's compiled text and font size"
chk '^page=00000000 content=1 connected=2,1$' "a compiled Page: its content, x:ConnectionId to the page"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
