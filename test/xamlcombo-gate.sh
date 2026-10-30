#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml ComboBox, ComboBoxItem, ToggleSwitch, InputScope(Name), CompositionTarget.Rendering
# and LayoutInformation (patches/sg/6198): a ComboBox from markup shows nothing selected; a click drops
# its list in a popup window and a click on an item selects it (SelectionChanged, SelectionBoxItem) and
# closes the list; Up on the focused box selects the item above; a ToggleSwitch flips on a click,
# raises Toggled and is drawn on; InputScopeName keeps its value, InputScope its Names; Rendering is
# raised every frame with its time; LayoutInformation gives an element's layout slot. Windows
# Terminal's settings pages (Startup and others) failed to be made without them. test/xamlcombo-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlcombo-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_COMBO_NO_DROP, SG_MUTANT_TOGGLE_SWITCH_NO_TOGGLED,
# SG_MUTANT_TOGGLE_SWITCH_NOT_DRAWN_ON, SG_MUTANT_NO_INPUT_SCOPE_NAME, SG_MUTANT_NO_RENDERING_FRAMES,
# SG_MUTANT_LAYOUT_SLOT_EMPTY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlcombo.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlcombo-probe.exe" "$HERE/xamlcombo-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-264}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/xamlcombo-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlcombo-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^combo=1 toggle=1$' "ComboBox and ToggleSwitch from markup"
chk '^initial=-1,0$' "nothing selected, the list up"
chk '^dropped=1 popup=1$' "a click drops the list down in a popup window"
chk '^chosen=1,0 changed=1$' "a click on an item selects it (SelectionChanged) and closes the list"
chk '^box_item=1$' "SelectionBoxItem"
chk '^key_up=0$' "Up on the focused box: the item above"
chk '^toggle=1 toggled=1 drawn=1$' "a ToggleSwitch: a click turns it on, Toggled, drawn on"
chk '^input_scope=29,1$' "InputScopeName keeps its value, InputScope its Names"
chk '^rendering=1 time=1$' "CompositionTarget.Rendering every frame, with a time"
chk '^layout_slot=0,0,300,200$' "LayoutInformation.GetLayoutSlot: the area its parent gave"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
