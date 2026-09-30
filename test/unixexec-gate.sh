#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Starting a Unix program leaks nothing (patches/sg/0584).
# NtCreateUserProcess returned early when it had started a Unix program and
# kept the image's handle and the working directory's descriptor: one of
# each at every start. Explorer runs a Unix helper (sg-lockctl) at every
# taskbar tick and held 20,000 handles after a day. The probe starts /bin/true
# 50 times; its handles and open descriptors must not grow.
#
#   WINE=/opt/wine-sg/bin/wine test/unixexec-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x /bin/true ] || { echo "SKIP: no /bin/true"; exit 77; }
T=$(mktemp -d /var/tmp/sg-unixexec.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/unixexec-probe.exe" "$HERE/unixexec-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/unixexec-probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout 60 "$WINE" 'C:\unixexec-probe.exe' 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
if [ "$out" = "handles=0 fds=0" ]; then
    echo "PASS  50 Unix programs started, no handle or descriptor kept"; echo "RESULT: PASS"; exit 0
fi
echo "FAIL  starting Unix programs leaks ($out)"; echo "RESULT: FAIL"; exit 1
