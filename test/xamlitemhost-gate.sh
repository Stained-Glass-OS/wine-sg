#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml for list items made by compiled templates (patches/sg/6168): weak references to XAML
# objects resolve while they live and not after, DataTemplate.SetExtensionInstance/GetExtensionInstance,
# conditional XAML blocks in a compiled object (test/mkxbf.py CondDict.xbf: the IsApiContractPresent
# block's Color set, the NotPresent one's not), a ListViewItemPresenter in a ControlTemplate shows the
# control's content. NanaZip's address bar suggestions. test/xamlitemhost-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlitemhost-gate.sh
# Mutants: SG_MUTANT_XAML_NO_WEAK_RESOLVE (object.c), SG_MUTANT_XBF_NO_CONDITIONAL_BLOCKS (xbf.c),
# SG_MUTANT_TEMPLATE_CONTENT_KEPT (control.c), SG_MUTANT_NO_TEMPLATE_EXTENSION (templates.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlitemhost.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlitemhost-probe.exe" "$HERE/xamlitemhost-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlitemhost-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlitemhost-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^weak=00000000,1,1$' "IWeakReferenceSource: resolves while alive, not after the last release"
chk '^extension=1,00000000,1$' "DataTemplate.SetExtensionInstance then GetExtensionInstance"
chk '^load=00000000$' "markup"
chk '^conditional=1$' "conditional XAML blocks: the Present block's Color, not the NotPresent one's"
chk '^presenter=1$' "a ListViewItemPresenter template shows the control's content"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
