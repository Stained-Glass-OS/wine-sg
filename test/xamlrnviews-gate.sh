#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What React Native for Windows asks of Windows.UI.Xaml to show its views (patches/sg/6211):
# AutomationProperties' attached LiveSetting/LabeledBy/IsRequiredForForm/AccessKey; the automation
# identifiers (AutomationElementIdentifiers, the patterns', ScrollPatternIdentifiers.NoScroll) and
# FrameworkElementAutomationPeer.FromElement; TextBox statics 2-8; ScrollViewer 2-4; FocusManager
# 3-6 (TryFocusAsync). OneDrive's sign-in page stopped at each. test/xamlrnviews-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlrnviews-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_AUTOMATION1_NOT_KEPT (buttons.c), SG_MUTANT_NO_AUTOMATION_IDENTIFIERS,
# SG_MUTANT_NO_PEER_STATICS (automation_ids.c), SG_MUTANT_NO_TEXT_BOX_STATICS2, SG_MUTANT_NO_SCROLL_VIEWER2
# (textinput.c), SG_MUTANT_NO_FOCUS_ASYNC (focus.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlrnviews.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlrnviews-probe.exe" "$HERE/xamlrnviews-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlrnviews-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlrnviews-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^attached=2,1,1,K$' "AutomationProperties: LiveSetting, LabeledBy, IsRequiredForForm, AccessKey kept"
chk '^identifiers=1,1,1$' "AutomationElementIdentifiers: one AutomationProperty each; the patterns' too"
chk '^noscroll=-1 peer=00000000,1$' "ScrollPatternIdentifiers.NoScroll; FromElement: no peer"
chk '^textbox_statics=1,1$' "TextBox's PlaceholderTextProperty and DescriptionProperty"
chk '^scrollviewer2=1,1,1,1$' "ScrollViewer 2: TopHeader kept, ViewChanging accepted, ChangeView taken"
chk '^try_focus=00000000,1,1,1,1$' "TryFocusAsync focuses the text box and completes at once"
chk '^focus_events=00000000,1$' "FocusManager.GotFocus accepted"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
