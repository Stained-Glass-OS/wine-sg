#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml hosting (patches/sg/6129): WindowsXamlManager, Window.Current over the thread's
# CoreWindow, DesktopWindowXamlSource (a XAML Island in a child window of its host) and Application,
# aggregated by an outer object as an application's App class does. There was no Windows.UI.Xaml
# at all: NanaZip stopped at Windows.UI.Xaml.Application. test/xamlhost-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlhost-gate.sh
# Mutant (dlls/windows.ui.xaml/application.c): SG_MUTANT_XAML_APP_NOT_AGGREGATED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlhost.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlhost-probe.exe" "$HERE/xamlhost-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlhost-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" xamlhost-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^manager=00000000$' "WindowsXamlManager.InitializeForCurrentThread"
chk '^manager_same=1$' "one per thread: a second call gets the same manager"
chk '^closable=00000000$' "the manager closes (IClosable)"
chk '^window=00000000 nonnull=1$' "the thread has a Window.Current"
chk '^window_private=00000000$' "with IWindowPrivate (island hosts set TransparentBackground)"
chk '^transparent=1$' "TransparentBackground is set and read back"
chk '^corewindow=00000000 nonnull=1$' "CoreWindow.GetForCurrentThread gives the thread's CoreWindow"
chk '^interop=00000000 hwnd=1$' "its ICoreWindowInterop window is a hidden window"
chk '^window_corewindow_same=1$' "Window.CoreWindow is that CoreWindow"
chk '^other_thread_window_null=1$' "a thread without XAML has no Window.Current"
chk '^source=00000000$' "a DesktopWindowXamlSource"
chk '^native=00000000$' "with IDesktopWindowXamlSourceNative"
chk '^attach=00000000$' "attached to a window"
chk '^island_child=1$' "its island is a child window of the host"
chk '^attach_again=8000ffff$' "attaching twice is refused (E_UNEXPECTED)"
chk '^content_null=1$' "its Content is set and read back"
chk '^app=00000000 inner=1$' "Application, aggregated by an outer object (its inner given back)"
chk '^app_delegates=1$' "its interfaces hand QueryInterface to the outer object"
chk '^current=00000000 same=1$' "Application.Current is it (through the outer object)"
chk '^theme=[01]$' "its RequestedTheme is the system's"
chk '^close=00000000$' "the manager's Close"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
