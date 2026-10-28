#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml classes WinUI 2 apps build on (patches/sg/6177): value helpers, Timeline statics,
# DispatcherTimer, PropertyChangedEventArgs, XamlCompositionBrushBase and AcrylicBrush, XamlLight and
# Lights, FlyoutBase, AppBarToggleButton, MenuFlyoutItem3/SubItem, ToolTip, IconSourceElement, BitmapIcon,
# ListViewItem/SelectorItem, AutomationProperties statics 2, SwapChainPanel's native interfaces, ScrollBar,
# Path, Dispatcher and DispatcherQueue, dictionary iteration, Loaded/LayoutUpdated of a hidden island --
# Windows Terminal and WinUI 2.8. test/xamlwinui2-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlwinui2-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_GRIDLENGTH_TYPE_IGNORED, SG_MUTANT_RECT_INTERSECT_UNION
# (helpers.c), SG_MUTANT_DURATION_NO_COMPARE, SG_MUTANT_TIMELINE_NOT_KEPT, SG_MUTANT_COMPOSITION_BRUSH_NO_FALLBACK
# (media.c), SG_MUTANT_TIMER_NO_TICK (timer.c), SG_MUTANT_PROPERTY_CHANGED_NO_NAME (dependency.c),
# SG_MUTANT_NO_XAML_LIGHT, SG_MUTANT_NO_LIGHTS (media.c, element.c), SG_MUTANT_SUB_ITEM_NO_TEXT (buttons.c),
# SG_MUTANT_ICON_SOURCE_NOT_SHOWN (icons.c), SG_MUTANT_NO_LIST_VIEW_ITEM_FACTORY (textinput.c),
# SG_MUTANT_AUTOMATION_NOT_KEPT (automation.c), SG_MUTANT_SWAP_CHAIN_REFUSED (panel.c),
# SG_MUTANT_PATH_NOT_PARSED (geometry.c), SG_MUTANT_XAML_NO_DISPATCHER (window.c),
# SG_MUTANT_XAML_NO_DISPATCHER_QUEUE (hosting.c), SG_MUTANT_DICT_NOT_ITERABLE (resources.c),
# SG_MUTANT_XAML_LOADED_ONLY_PAINTED (hosting.c), SG_MUTANT_XAML_NO_LAYOUT_UPDATED (element.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlwinui2.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlwinui2-probe.exe" "$HERE/xamlwinui2-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid -lm || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlwinui2-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlwinui2-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^gridlength=2,2,1$' "GridLengthHelper.FromValueAndType: 2*, a star length"
chk '^rect=5,5,5,5,1$' "RectHelper.Intersect; RectHelper.Empty is empty"
chk '^size=1$' "SizeHelper.Empty: -infinity wide"
chk '^duration=-1,1,30000000$' "DurationHelper: Automatic before a time span; 1 s + 2 s = 3 s"
chk '^timeline=0$' "Timeline.AllowDependentAnimations kept"
chk '^timer=1,1,0,1$' "DispatcherTimer: enabled, ticks every interval on the thread's loop, none after Stop"
chk '^propertychanged=Title$' "PropertyChangedEventArgs.PropertyName"
chk '^compositionbrush=1,ffff0000$' "XamlCompositionBrushBase made; its FallbackColor kept"
chk '^acrylic=1,ff0000ff,1$' "AcrylicBrush from markup: TintColor, a XamlCompositionBrushBase"
chk '^lights=1,1$' "a XamlLight in a Grid's Lights"
chk '^flyoutbase=1,1$' "FlyoutBase made; FlyoutBase.AttachedFlyout set and read back"
chk '^appbartoggle=1,1$' "AppBarToggleButton: a toggle button and a command bar element"
chk '^menuitem3=Ctrl.C$' "MenuFlyoutItem.KeyboardAcceleratorTextOverride"
chk '^subitem=More,2$' "MenuFlyoutSubItem: Text, its Items from markup"
chk '^subitemset=Less$' "MenuFlyoutSubItem: Text set"
chk '^tooltip=1,1$' "ToolTip from markup; IsOpen kept"
chk '^iconsource=1,1$' "IconSourceElement shows its FontIconSource (a child)"
chk '^bitmapicon=1,1$' "BitmapIcon shows its UriSource (a child image)"
chk '^listviewitem=1,1$' "a program's ListViewItem; SelectorItem.IsSelected kept"
chk '^automation=0,2$' "AutomationProperties.AccessibilityView from markup (Raw) and set/read back"
chk '^swapchainpanel=1,1,00000000,00000000$' "SwapChainPanel: ISwapChainPanelNative/Native2, SetSwapChainHandle and SetSwapChain taken"
chk '^scrollbar=1,10$' "ScrollBar from markup: Orientation, ViewportSize"
chk '^path=1,2,3,10,10$' "Path Data from path markup (relative and H segments): its geometry's bounds"
chk '^dispatchers=1,1$' "a dependency object's Dispatcher; the thread's DispatcherQueue"
chk '^dictionary=2,1$' "a ResourceDictionary's entries iterated, keys given"
chk '^hidden=0,1,1$' "content of an island not shown: Loaded and LayoutUpdated raised"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
