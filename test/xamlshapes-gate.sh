#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml shapes and text styles (patches/sg/6149): Rectangle (Fill, Stroke, rounded
# corners), Ellipse and Line drawn in a XAML Island; the system text styles programs base theirs on
# (CaptionTextBlockStyle, SubtitleTextBlockStyle -- NanaZip's status bar); a compiled dictionary's
# ThemeDictionaries (owner-named member, keyed item; test/mkxbf.py) found by {ThemeResource}.
# test/xamlshapes-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlshapes-gate.sh
# Mutant (dlls/windows.ui.xaml/shapes.c): SG_MUTANT_SHAPE_NO_FILL.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlshapes.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlshapes-probe.exe" "$HERE/xamlshapes-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlshapes-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlshapes-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with shapes, text styles and a compiled theme dictionary"
chk '^caption=12$' "CaptionTextBlockStyle"
chk '^subtitle=20,600$' "SubtitleTextBlockStyle (size, weight)"
chk '^rect_fill=1$' "a Rectangle's Fill"
chk '^rect_stroke=1,1$' "a Rectangle's Stroke, inside hollow"
chk '^rounded=1,1$' "RadiusX/RadiusY: rounded corners"
chk '^ellipse=1,1$' "an Ellipse"
chk '^line=1,1$' "a Line"
chk '^theme_dictionary=1$' "{ThemeResource} from a compiled ThemeDictionaries entry"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
