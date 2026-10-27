#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml resources and styles (patches/sg/6144): ResourceDictionary (element and application
# resources), Style with Setters and BasedOn, keyed and implicit styles, a style's values below local
# values, {StaticResource} and {ThemeResource} -- the desktop's own theme resources by their usual
# names. Pages and controls of programs are styled this way. test/xamlstyles-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlstyles-gate.sh
# Mutant (dlls/windows.ui.xaml/resources.c): SG_MUTANT_STYLE_NO_SETTERS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlstyles.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlstyles-probe.exe" "$HERE/xamlstyles-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlstyles-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlstyles-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'app_resources=1 insert=00000000' "Application.Resources: a resource inserted from code"
chk 'load=00000000' "markup with resources and styles"
chk 'style=40,ff0000ff' "Style=\"{StaticResource Big}\": its setters, a brush resource among them"
chk 'local_wins=10' "a local value beats the style's"
chk 'based_on=20,ff0000ff' "BasedOn: the base style's setters, then its own"
chk 'theme_resource=1' "{ThemeResource TextFillColorPrimaryBrush}: the desktop's text colour"
chk 'app_resource=ff008000' "{StaticResource} found in the application's resources"
chk 'implicit=50x50' "an implicit style (keyed by its TargetType) applied to a Border"
chk 'lookup=00000000 brush=1' "Resources.Lookup from code"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
