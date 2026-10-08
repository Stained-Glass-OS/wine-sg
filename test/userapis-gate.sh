#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# user32 functions that were stubs (patches/sg/1604), under Xvfb:
# test/userapis-probe.c. SetWindowDisplayAffinity failed
# (ERROR_NOT_ENOUGH_MEMORY: Zoom, Teams and password managers keep windows
# out of captures with it); GetUserObjectSecurity gave an empty descriptor
# (no DACL); RegisterPowerSettingNotification returned 0xdeadbeef and never
# sent the current value; EnableNonClientDpiScaling failed.
#
#   WINE=/opt/wine-sg/bin/wine test/userapis-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_DISPLAY_AFFINITY (user32/win.c),
# SG_MUTANT_NO_USER_OBJECT_SECURITY (user32/winstation.c),
# SG_MUTANT_NO_POWER_NOTIFY (user32/misc.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${USERAPIS_DPY:-361}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-userapis.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/userapis-probe.exe" "$HERE/userapis-probe.c" \
    -lgdi32 -luser32 -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/userapis-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
