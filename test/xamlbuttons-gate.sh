#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml buttons and command bars (patches/sg/6140): CommandBar, AppBarButton (Icon by
# symbol name, Label, content as its icon), AppBarSeparator, Button, AutomationProperties and
# ToolTipService from markup, drawn in the desktop's look (system colours; symbols in a standard font
# when the symbol font is missing). NanaZip's toolbar is made of these. test/xamlbuttons-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlbuttons-gate.sh
# Mutant (dlls/windows.ui.xaml/buttons.c): SG_MUTANT_BUTTON_NO_ICON.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlbuttons.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlbuttons-probe.exe" "$HERE/xamlbuttons-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlbuttons-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlbuttons-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "a CommandBar of AppBarButtons and a Button from markup"
chk '^primary_commands=3$' "CommandBar.PrimaryCommands (its content)"
chk '^label=Add icon=e109$' "AppBarButton Label; Icon=\"Add\" is a SymbolIcon (Symbol.Add)"
chk '^automation_name=Add it tooltip=1$' "AutomationProperties.Name and ToolTipService.ToolTip (attached)"
chk '^button_size=1$' "a Button sized to its text and padding"
chk '^add_icon=1$' "the Add icon drawn"
chk '^content_icon=1$' "an AppBarButton's content drawn as its icon (a symbol-font glyph standing in)"
chk '^separator=1$' "the AppBarSeparator's line"
chk '^button_face=1 button_text=1$' "the Button's face in the desktop's control colour, its text on it"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
