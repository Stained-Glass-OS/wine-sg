#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml SwapChainPanel presenting (patches/sg/6178): a composition swap chain made for a
# DirectComposition surface handle (IDXGIFactoryMedia, as Windows Terminal's renderer makes it) given by
# SetSwapChainHandle, and one given by SetSwapChain: presented frames show in the panels' places in a
# hosted island, again after the island paints, and at once when a new frame comes.
# test/xamlswapchain-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlswapchain-gate.sh
# Mutants: SG_MUTANT_DXGI_NO_FACTORY_MEDIA (dlls/dxgi/factory.c), SG_MUTANT_DXGI_SURFACE_HANDLE_UNKNOWN
# (dlls/dxgi/composition.c), SG_MUTANT_SWAP_PANEL_NOT_PRESENTED (dlls/windows.ui.xaml/panel.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlswapchain.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlswapchain-probe.exe" "$HERE/xamlswapchain-probe.c" -lruntimeobject -ld3d11 -ldxgi -ldcomp -luser32 -lgdi32 -luuid -lm || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlswapchain-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlswapchain-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^media=1,1,00000000,1$' "IDXGIFactoryMedia makes a composition swap chain for a surface handle (none without one)"
chk '^load=00000000$' "markup with two SwapChainPanels"
chk '^set=00000000,00000000$' "SetSwapChainHandle and SetSwapChain taken"
chk '^byhandle=1,1$' "the frame presented to the handle's swap chain shows in its panel's place, not beyond"
chk '^bychain=1,1$' "the frame of the swap chain set shows in its panel's place, not beyond"
chk '^repaint=1,1$' "both shown again after the island paints over them"
chk '^next=1$' "a new frame shows at once"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
