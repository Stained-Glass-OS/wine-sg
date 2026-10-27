#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml text inlines (patches/sg/6143): a TextBlock's Inlines -- Run, LineBreak, Span -- from
# markup and code, laid out (line breaks as lines) and laid out again when a Run's Text changes.
# NanaZip's About page sets its version in Runs and crashed without them. test/xamlinlines-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlinlines-gate.sh
# Mutant (dlls/windows.ui.xaml/documents.c): SG_MUTANT_INLINES_NO_TEXT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlinlines.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlinlines-probe.exe" "$HERE/xamlinlines-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlinlines-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlinlines-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'load=00000000' "a TextBlock of Runs, a LineBreak and a Span from markup"
chk 'inlines=4' "its Inlines"
chk 'line_break=1' "a LineBreak lays out as a new line"
chk 'run_text=1 relaid=1' "a Run's Text set from code: the TextBlock laid out again"
chk 'code_inline=00000000 width=1' "a Run appended to Inlines from code"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
