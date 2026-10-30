#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml pieces WinUI 2's NavigationView needs for its menu (patches/sg/6197): an element in a
# ScrollViewer is told its EffectiveViewportChanged and the ScrollViewer is an IScrollAnchorProvider
# (ItemsRepeater realizes its items from them); FindName reaches a ContentControl's content before it
# is shown; a compiled {TemplateBinding} on AutomationProperties.Name sets that, not the part's Name;
# x:Uid in a library's markup (ms-appx:///Lib/...) takes the library's strings (Lib/Resources), not
# the application's; FontIcon statics 2 and 3 are the instance's properties; PathIcon draws its
# geometry; ThemeShadow keeps its Receivers and UIElement.Shadow keeps it; BindingOperations.SetBinding
# with RelativeSource TemplatedParent on a template part; UIElement's routed event identifiers, and
# AddHandler(KeyDownEvent) is called for a key. Windows Terminal's settings page showed an empty
# menu without them. test/xamlnavmenu-probe.c, fixtures from test/mkxbf.py and test/mkpri.py.
#   WINE=/opt/wine-sg/bin/wine test/xamlnavmenu-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_NO_VIEWPORT_CHANGED, SG_MUTANT_NO_SCROLL_ANCHOR_PROVIDER,
# SG_MUTANT_NAME_NOT_IN_PENDING_CONTENT, SG_MUTANT_TEMPLATE_BIND_NO_OWNER, SG_MUTANT_UID_NO_LIBRARY_MAP,
# SG_MUTANT_NO_FONT_ICON_STATICS3, SG_MUTANT_PATH_ICON_NO_SIZE, SG_MUTANT_NO_THEME_SHADOW,
# SG_MUTANT_NO_SET_BINDING, SG_MUTANT_ADD_HANDLER_NOT_JOINED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlnavmenu.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlnavmenu-probe.exe" "$HERE/xamlnavmenu-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlnavmenu-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlnavmenu-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^pending_name=1$' "FindName reaches a ContentControl's content not shown yet"
chk '^pending_shown=1$' "and a shown ContentControl's content its template does not show"
chk '^anchor_provider=1$' "a ScrollViewer is an IScrollAnchorProvider"
chk '^routed_events=1,1,1 add_handler=00000000$' "UIElement's routed event identifiers (one object each); AddHandler"
chk '^viewport=1 200x100$' "EffectiveViewportChanged: the ScrollViewer's viewport"
chk '^key_handler=1$' "a key reaches the handler AddHandler(KeyDownEvent) added"
chk '^path_icon=1 size=140x10 drawn=1$' "PathIcon: its geometry's size (and its margin), drawn in its Foreground"
chk '^font_icon_statics=1,1 mirrored=1$' "FontIcon statics 2 and 3: the instance's properties"
chk '^theme_shadow=00000000 receivers=1 shadow=1$' "ThemeShadow keeps its Receivers; UIElement.Shadow keeps it"
chk '^set_binding=00000000 width=77$' "BindingOperations.SetBinding, RelativeSource TemplatedParent, on a template part"
chk '^templ_owner=00000000 name=Frame automation=Btn$' "a compiled {TemplateBinding} on AutomationProperties.Name"
chk '^library_uid=00000000 Library text$' "x:Uid in a library's markup: the library's strings"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
