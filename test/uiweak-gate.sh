#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A thread's CoreDispatcher and a UISettings can be held weakly (patches/sg/6203).
# C++/WinRT programs keep them as weak references (React Native for Windows, in OneDrive); both
# answered E_NOINTERFACE for IWeakReferenceSource and OneDrive crashed. test/uiweak-probe.c (the
# XAML switch on, for the thread's CoreWindow).
#   WINE=/opt/wine-sg/bin/wine test/uiweak-gate.sh
# Mutants (dlls/windows.ui): SG_MUTANT_DISPATCHER_NO_WEAK_SOURCE (dispatcher.c),
# SG_MUTANT_UISETTINGS_NO_WEAK_SOURCE (uisettings.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-uiweak.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/uiweak-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(WINE_SG_XAML=1 timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'dispatcher_weak=1' "a thread's CoreDispatcher gives a weak reference (IWeakReferenceSource)"
chk 'dispatcher_resolves=1' "which resolves to the dispatcher"
chk 'settings=00000000' "a UISettings is made"
chk 'settings_weak=1' "a UISettings gives a weak reference"
chk 'settings_resolves=1' "which resolves to it while it lives"
chk 'settings_gone=1' "and to nothing once it is gone"
chk 'done=1' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
