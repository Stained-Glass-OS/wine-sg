#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml list pieces Windows Terminal's TabView needs (patches/sg/6184): the ListViewBase
# statics and ListViewBase2..5 (Footer), Selector.SelectedIndexProperty following SelectedIndex,
# IItemContainerMapping, a ListViewItem among the items as its own container, and a control's
# DefaultStyleResourceUri (a library's generic.xaml) giving its default style. test/xamllistbase-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamllistbase-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_SELECTOR_PROPS_STILL, SG_MUTANT_NO_CONTAINER_MAPPING,
# SG_MUTANT_ITEM_NOT_OWN_CONTAINER (textinput.c), SG_MUTANT_NO_DEFAULT_STYLE_URI (resources.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamllistbase.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamllistbase-probe.exe" "$HERE/xamllistbase-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamllistbase-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamllistbase-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^lvb_statics=1,1,1$' "ListViewBase statics: HeaderProperty, FooterProperty, IListViewBaseStatics5"
chk '^load=00000000$' "markup"
chk '^footer=1,1$' "ListViewBase2.Footer; IListViewBase5"
chk '^default_style=00000000,77$' "DefaultStyleResourceUri: the default style from that dictionary"
chk '^selected_index=1,1$' "SelectedIndexProperty follows SelectedIndex"
chk '^mapping=1,1$' "ContainerFromIndex, IndexFromContainer"
chk '^own_container=1$' "a ListViewItem item is its own container"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
