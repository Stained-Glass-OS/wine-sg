#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml Frame (patches/sg/6193): Navigate makes the page of a type (the application's or
# ours) with its parameter, shows it, sets its Frame, raises Navigated (NavigationEventArgs) and keeps
# the pages left for GoBack/GoForward; <TransitionCollection> with NavigationThemeTransition and the
# navigation transition infos in markup (kept, not animated); a merged dictionary's relative Source
# is relative to the file naming it -- Windows Terminal's settings page (MainPage.xaml: Frame
# contentFrame, CommonResources.xaml). test/xamlframe-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlframe-gate.sh
# Mutants: SG_MUTANT_FRAME_NO_CONTENT, SG_MUTANT_FRAME_NO_BACK_STACK (frame.c),
# SG_MUTANT_MARKUP_NO_COLLECTION_ITEMS, SG_MUTANT_SOURCE_NOT_RELATIVE (markup.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlframe.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlframe-probe.exe" "$HERE/xamlframe-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlframe-probe.exe" "$WINEPREFIX/drive_c/"
NSP='xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"'
mkdir -p "$WINEPREFIX/drive_c/Sub"
cat > "$WINEPREFIX/drive_c/Sub/Outer.xaml" <<EOF
<ResourceDictionary $NSP><ResourceDictionary.MergedDictionaries><ResourceDictionary Source="Inner.xaml"/><ResourceDictionary Source="../Top.xaml"/></ResourceDictionary.MergedDictionaries></ResourceDictionary>
EOF
cat > "$WINEPREFIX/drive_c/Sub/Inner.xaml" <<EOF
<ResourceDictionary $NSP><SolidColorBrush x:Key="InnerBrush" Color="#FF0000FF"/></ResourceDictionary>
EOF
cat > "$WINEPREFIX/drive_c/Top.xaml" <<EOF
<ResourceDictionary $NSP><SolidColorBrush x:Key="TopBrush" Color="#FF00FF00"/></ResourceDictionary>
EOF
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlframe-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^frame=00000000$' "a Frame"
chk '^navigate=00000000 ok=1$' "Navigate(Page, parameter)"
chk '^content_page=1$' "its Content is the page made"
chk '^page_frame_is_frame=1$' "the page's Frame is the Frame"
chk '^navigated=1 mode=0 type=Windows.UI.Xaml.Controls.Page param=1 content_same=1$' "Navigated: mode New, the type, the parameter, the page"
chk '^navigate2=00000000 ok=1 new_page=1$' "INavigate.Navigate: a new page"
chk '^two back=1 forward=0 depth=1$' "the page left is on the back stack"
chk '^back=00000000 navigated=3 mode=1 param=1$' "GoBack: mode Back, the first page's parameter"
chk '^after_back back=0 forward=1 depth=0$' "and the page left goes forward"
chk '^forward=00000000 mode=2$' "GoForward"
chk '^bad=00000000 ok=0$' "a type that cannot be made: Navigate gives false"
chk '^load=00000000$' "markup"
chk '^markup_frame=1$' "<Frame> in markup"
chk '^transitions=1 navigation_transition=1 drill_in=1$' "ContentTransitions: a TransitionCollection, NavigationThemeTransition, DrillInNavigationTransitionInfo"
chk '^relative=1 up=1$' "a merged dictionary's Source relative to the file naming it (Inner.xaml, ../Top.xaml)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
