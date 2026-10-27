#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml element lifetime and compiled templates' connections (patches/sg/6151): Loading,
# Loaded and Unloaded as an element enters and leaves an island's tree, IsLoaded; a compiled
# ControlTemplate's x:ConnectionId reaches the component whose markup held it (IComponentConnector)
# and its x:Bind connector (IComponentConnector2) -- how applications' compiled x:Bind in templates is
# wired (NanaZip's status and address bars), XamlBindingHelper (what compiled x:Bind code calls);
# framework IDs newer than our table are dropped, not
# fatal (test/mkxbf.py TemplConn.xbf). test/xamlloaded-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlloaded-gate.sh
# Mutant (dlls/windows.ui.xaml/element.c): SG_MUTANT_XAML_NO_LOADED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlloaded.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlloaded-probe.exe" "$HERE/xamlloaded-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlloaded-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlloaded-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^before=0,0,0$' "not loaded before it is in a live tree"
chk '^loaded=1,1,1 sender=1$' "Loading and Loaded once in an island, IsLoaded"
chk '^unloaded=1,0$' "Unloaded when removed"
chk '^bind_set=00000000,bound,31$' "XamlBindingHelper.SetPropertyFromString/Double (compiled x:Bind)"
chk '^template_component=1$' "XamlBindingHelper.Set/GetDataTemplateComponent"
chk '^load=00000000$' "compiled markup with newer framework IDs loads"
chk '^binding_connector=2,1$' "IComponentConnector2.GetBindingConnector(the template's own ID, the control)"
chk '^connected=5,5$' "the component and its binding connector connect the template's element"
chk '^scope_connected=2$' "the x:Bind scope connected to its data root, the control (Connect(2, control))"
chk '^template=1$' "the template drawn ({TemplateBinding})"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
