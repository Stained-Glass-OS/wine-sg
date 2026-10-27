#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml text and media (patches/sg/6135): TextBlock laid out with DirectWrite -- Text,
# FontSize, TextWrapping, Padding, the Text property -- SolidColorBrush (Color, Opacity) as its
# Foreground, FontFamily. There were none. test/xamltext-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamltext-gate.sh
# Mutant (dlls/windows.ui.xaml/text.c): SG_MUTANT_TEXT_NO_WRAP.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamltext.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamltext-probe.exe" "$HERE/xamltext-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamltext-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamltext-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qxF "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'hello=1 line=1 longer=1 same_height=1' "TextBlock: its text's size (a line; a longer text is wider)"
chk 'fontsize_scales=1' "FontSize 28 lays out twice as large"
chk 'wraps=1' "TextWrapping Wrap: within the width, more lines"
chk 'padding=10,10' "Padding around the text"
chk 'text=Hello' "Text"
chk 'textproperty=Hello' "TextProperty through the property system"
chk 'brush=ffc80a14 opacity=1.0' "SolidColorBrush(Color), Opacity 1"
chk 'foreground=1' "Foreground"
chk 'family=Courier New' "FontFamily(name).Source"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
