#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml for an island host's own dialogs (patches/sg/6173): a default Application (Current, its
# Resources falling back on the framework's), Canvas, UIElement.XamlRoot, VisualTreeHelper, Transitions
# kept, a BitmapImage from a file name, KeyboardAccelerators, VisualStateManager.GoToState, FontWeights.
# Advanced Installer's XAML dialogs (TeraCopy's installer). test/xamlhostapp-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlhostapp-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_XAML_NO_DEFAULT_APP (application.c),
# SG_MUTANT_XAML_APP_DICT_NO_FALLBACK (resources.c), SG_MUTANT_CANVAS_NO_PLACE (panel.c),
# SG_MUTANT_XAML_NO_XAMLROOT (hosting.c), SG_MUTANT_XAML_VTH_NO_CHILDREN, SG_MUTANT_XAML_NO_TRANSITIONS
# (element.c), SG_MUTANT_IMAGE_NO_FILE_NAME (imaging.c), SG_MUTANT_XAML_NO_ACCELERATORS (accel.c),
# SG_MUTANT_XAML_NO_VSM (templates.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlhostapp.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlhostapp-probe.exe" "$HERE/xamlhostapp-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlhostapp-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlhostapp-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^app=1,1$' "Application.Current made when the thread's XAML is initialized, with Resources"
chk '^appres=00000000,1,1$' "its Resources find the framework's (SystemColorButtonFaceColor)"
chk '^load=00000000$' "markup with a Canvas"
chk '^canvas=1,30,1$' "Canvas.Left/Top place an element and move it"
chk '^xamlroot=1,1,300$' "UIElement.XamlRoot: the island's content and size"
chk '^vth=3,1,1$' "VisualTreeHelper: children, child, parent"
chk '^transitions=1,1,1$' "UIElement.Transitions keeps a theme transition"
chk '^image=1$' "an Image's Source given as a file name"
chk '^accel=0,1$' "a KeyboardAccelerator (F5) clicks its check box"
chk '^misc=1,700$' "VisualStateManager.GoToState; FontWeights.Bold"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
