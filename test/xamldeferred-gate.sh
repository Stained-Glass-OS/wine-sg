#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml compiled markup of WinUI 2 apps (patches/sg/6177): x:Load elements (DeferredElement with
# a section reference of kind 9) made when FindName asks for them, in their place; a root object node
# inside the tree; x:Name on objects without a Name; Silverlight's markup namespace. Fixtures from
# test/mkxbf.py (DeferLoad, NestedRoot, NamedBrush). test/xamldeferred-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamldeferred-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_XAML_NO_DEFER_REALIZE, SG_MUTANT_XAML_NO_DEFER,
# SG_MUTANT_XAML_DEFER_APPENDED, SG_MUTANT_XBF_NESTED_ROOT_ENDS (xbf.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamldeferred.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamldeferred-probe.exe" "$HERE/xamldeferred-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid -lm || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamldeferred-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamldeferred-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^deferload=00000000,1,1,1,2,1$' "an x:Load element: not made with the page; made when found by name, once"
chk '^placed=1$' "put where it was (before the element that follows it)"
chk '^unknown=1$' "a name not in the page: nothing"
chk '^bare=00000000,1$' "a kind 9 reference outside a DeferredElement: still left unrealized"
chk '^nestedroot=00000000,1,1$' "an object written as a root inside the tree: the reading goes on after it"
chk '^namedbrush=00000000,1$' "an x:Name on a brush: the page loads"
chk '^silverlight=00000000,1$' "markup in Silverlight's namespace"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
