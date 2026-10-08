#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Small stubs that answered wrongly (patches/sg/1622), under Xvfb:
# test/guithread-probe.c. IsGUIThread said TRUE for every thread; uxtheme's
# app/window dark mode state (133/135/137, 139 missing) kept nothing;
# ABM_SETAUTOHIDEBAR registered no program's auto-hide bar; AppPolicyGet*
# took any handle; GetProfileType took NULL; NetGetAadJoinInformation
# failed with ERROR_CALL_NOT_IMPLEMENTED (Office asks it at start).
#
#   WINE=/opt/wine-sg/bin/wine test/guithread-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_GUI_THREAD_ALWAYS (win32u/message.c),
# SG_MUTANT_NO_DARK_WINDOW_STATE (uxtheme/system.c),
# SG_MUTANT_NO_AUTOHIDE_BARS (programs/explorer/appbar.c),
# SG_MUTANT_NO_APP_POLICY_TOKEN (kernelbase/main.c),
# SG_MUTANT_AAD_NOT_IMPLEMENTED (netapi32/netapi32.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${GUITHREAD_DPY:-397}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-guithread.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/guithread-probe.exe" "$HERE/guithread-probe.c" \
    -luser32 -lshell32 -ladvapi32 -lgdi32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/guithread-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
