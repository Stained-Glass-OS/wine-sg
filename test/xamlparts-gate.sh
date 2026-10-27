#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml parts of NanaZip's address bar (patches/sg/6154): TextCommandBarFlyout made and
# placed (its OnApplyTemplate stopped there), a Popup's child in its name scope (GetTemplateChild of
# the suggestions list), ListView selection, a ScrollViewer showing its content whatever template an
# implicit style gives it, a deferred element of compiled markup (section reference kind 9, test/mkxbf.py)
# left unrealized. test/xamlparts-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlparts-gate.sh
# Mutant (dlls/windows.ui.xaml/element.c): SG_MUTANT_XAML_NO_POPUP_SCOPE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlparts.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlparts-probe.exe" "$HERE/xamlparts-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlparts-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlparts-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^flyout=00000000,5,0$' "TextCommandBarFlyout: made, its Placement, its commands"
chk '^load=00000000$' "markup with a ScrollViewer under an implicit style and a Popup"
chk '^popup_child=1$' "FindName finds a Popup's child"
chk '^unselected=-1$' "a ListView selects nothing at first"
chk '^selection=1,1$' "SelectedIndex selects its item; past the items fails"
chk '^deferred=00000000,1,1$' "a deferred element (kind 9) left unrealized, the rest made"
chk '^scroll_viewer=1,1$' "a ScrollViewer shows its content, not a template we cannot fill"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
