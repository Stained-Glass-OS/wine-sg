#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml markup (patches/sg/6138): XAML text through XamlReader.Load and
# Application.LoadComponent -- attribute values by type, property elements, attached and content
# properties, x:Name, x:ConnectionId handed to the component -- drawn in a XAML Island; malformed
# markup (unclosed, a bad value, not XML, nested too deep) fails without harm. test/xamlmarkup-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlmarkup-gate.sh
# Mutant (dlls/windows.ui.xaml/markup.c): SG_MUTANT_MARKUP_NO_ATTACHED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlmarkup.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlmarkup-probe.exe" "$HERE/xamlmarkup-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlmarkup-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlmarkup-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "XamlReader.Load of a Grid with rows, columns, a Border, text and a StackPanel"
chk '^findname=1$' "x:Name (FindName)"
chk '^unclosed=1$' "an unclosed element fails"
chk '^bad_value=1$' "a value of the wrong type fails"
chk '^not_xml=1$' "text that is not XML fails"
chk '^too_deep=1$' "markup nested too deep fails, without harm"
chk '^background=1$' "Background=\"Red\" drawn"
chk '^auto_column=1$' "an Auto column sized to its content"
chk '^border=1,1$' "the Border where its attached Grid.Row/Column, Margin and size put it, its colours from #hex and names"
chk '^text=1$' "the TextBlock's text"
chk '^loadcomponent=00000000 content=1 connected=2,1$' "LoadComponent: a page's markup onto the page, x:ConnectionId to the page"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
