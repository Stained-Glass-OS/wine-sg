#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml toggles and split buttons (patches/sg/6173): CheckBox two and three states (box filled
# when checked, Checked/Unchecked), RadioButton by parent and by GroupName, ToggleButton, a SplitButton's
# primary part and its chevron opening its MenuFlyout (MenuFlyoutItem Click, separators not entries).
# Advanced Installer's XAML dialogs (TeraCopy's installer). test/xamltoggle-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamltoggle-gate.sh
# Mutants: SG_MUTANT_TOGGLE_NO_TOGGLE, SG_MUTANT_RADIO_NO_GROUP, SG_MUTANT_SPLIT_BUTTON_NO_MENU,
# SG_MUTANT_MENU_ITEM_NO_CLICK (dlls/windows.ui.xaml/buttons.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamltoggle.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamltoggle-probe.exe" "$HERE/xamltoggle-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamltoggle-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamltoggle-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with check boxes, radio buttons, a toggle and a split button"
chk '^check=0,1,1,1$' "a click checks a check box; its box filled"
chk '^uncheck=0,1,1,1$' "a second click unchecks it; Checked and Unchecked raised once each"
chk '^three=1,-1,0$' "three states: checked, indeterminate, unchecked"
chk '^radio=1,0,1$' "a radio button unchecks its parent's other one"
chk '^radio_again=1$' "a checked radio button stays checked"
chk '^group=1,0,1,1$' "a GroupName's radio buttons across parents; other groups untouched"
chk '^toggle=1,0$' "a toggle button"
chk '^primary=1,0$' "a split button's primary part clicks it, no menu"
chk '^chevron=1,1,1$' "its chevron opens its menu of two entries (a separator is not one)"
chk '^item=1,0,1$' "an entry chosen: its Click, the menu closed"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
