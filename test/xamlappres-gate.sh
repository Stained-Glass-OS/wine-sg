#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml application resources (patches/sg/6147): an application's App.xaml loaded for it
# when its App does not load it itself (as NanaZip's does not) before its first markup -- its
# resources, a merged dictionary from a text Source, one from a compiled Source as newer compilers
# write them (dictionary kind 10, conditional XAML, types by XML namespace; test/mkxbf.py) -- used by
# {StaticResource} in markup drawn in a XAML Island; the desktop's system colours as resources; a
# dictionary that is its own Source fails without harm. test/xamlappres-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlappres-gate.sh
# Mutant (dlls/windows.ui.xaml/application.c): SG_MUTANT_XAML_NO_APP_MARKUP.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlappres.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlappres-probe.exe" "$HERE/xamlappres-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlappres-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
NSP='xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"'
cat > "$WINEPREFIX/drive_c/App.xaml" <<EOF
<Application $NSP><Application.Resources><ResourceDictionary>
<ResourceDictionary.MergedDictionaries><ResourceDictionary Source="Styles.xaml"/><ResourceDictionary Source="ms-appx:///AppDict.xbf"/></ResourceDictionary.MergedDictionaries>
<SolidColorBrush x:Key="AppBrush" Color="#FF00FF00"/>
</ResourceDictionary></Application.Resources></Application>
EOF
cat > "$WINEPREFIX/drive_c/Styles.xaml" <<EOF
<ResourceDictionary $NSP><SolidColorBrush x:Key="StyleBrush" Color="#FF0000FF"/></ResourceDictionary>
EOF
cat > "$WINEPREFIX/drive_c/Loop.xaml" <<EOF
<ResourceDictionary $NSP Source="ms-appx:///Loop.xaml"/>
EOF
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlappres-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^app=1$' "the application"
chk '^loop=1$' "a dictionary that is its own Source fails"
chk '^load=00000000$' "markup using the application's resources"
chk '^app_xaml=1$' "App.xaml loaded for the application: its resource"
chk '^text_source=1$' "a merged dictionary from a text Source"
chk '^compiled_source=1$' "a merged dictionary from a compiled Source (dictionary kind 10, type by XML namespace)"
chk '^conditional=1$' "conditional XAML: the alternative whose predicates hold"
chk '^system_color=1$' "{StaticResource SystemColorHighlightColor}: the desktop's highlight colour"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
