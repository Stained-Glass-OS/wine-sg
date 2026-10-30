#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml SizeChanged and combo box follow-ups (patches/sg/6200): FrameworkElement.SizeChanged
# after a layout pass that changed an element's RenderSize (previous and new size); a ComboBox reads an
# ItemsSource that is only an IBindableVector (C++/WinRT's typed vectors are that to XAML); a SelectedItem
# given before the items waits for them, and a boxed string matches an item of the same text. WinUI 2's
# NavigationView picks its pane mode on SizeChanged (Windows Terminal's settings page was laid out under
# its pane); Windows Terminal's language and profile combo boxes showed nothing. test/xamlsizechanged-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlsizechanged-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_NO_SIZE_CHANGED, SG_MUTANT_NO_BINDABLE_ITEMS,
# SG_MUTANT_ITEM_INDEX_POINTER_ONLY, SG_MUTANT_COMBO_NO_PENDING.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlsizechanged.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlsizechanged-probe.exe" "$HERE/xamlsizechanged-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlsizechanged-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlsizechanged-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^selected_early=00000000$' "a SelectedItem before the items is taken"
chk '^size_changed=1 0x0->100x50$' "SizeChanged after the first layout: from 0x0"
chk '^size_changed2=2 100x50->120x50$' "and again when its Width changes"
chk '^combo_selected=1$' "the combo box reads an IBindableVector; the early SelectedItem (an equal string) is chosen"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
