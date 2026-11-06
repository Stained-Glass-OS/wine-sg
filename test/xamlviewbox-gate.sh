#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml Viewbox (patches/sg/6263): the child laid out at the size it wants and drawn scaled to
# the Viewbox's by Stretch (Uniform by default, Fill, None) and StretchDirection (DownOnly), in the
# middle; the class activates, its defaults (Uniform, Both), a Stretch set and read back, its statics.
# Windows Terminal's markup has Viewboxes (WinUI 2), which had no type. test/xamlviewbox-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlviewbox-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_VIEWBOX_NO_SCALE (panel.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlviewbox.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlviewbox-probe.exe" "$HERE/xamlviewbox-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlviewbox-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlviewbox-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^defaults=1 stretch=2 direction=2$' "the class activates; Stretch Uniform and StretchDirection Both by default"
chk '^set_stretch=1$' "Stretch set (Fill) and read back"
chk '^statics=1$' "ViewboxStatics.StretchProperty"
chk '^load=00000000$' "markup with Viewboxes loads"
chk '^uniform=11111$' "Uniform: the 20x10 child drawn 100x50, in the middle"
chk '^fill=111$' "Fill: drawn over the whole Viewbox"
chk '^none=1111$' "None: drawn at its own size, in the middle"
chk '^downonly=111$' "StretchDirection DownOnly: not made larger"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
