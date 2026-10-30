#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Globalization.ApplicationLanguages (patches/sg/6206).
# The class did not exist: React Native for Windows (OneDrive) asks it for the languages to show
# itself in and stopped. test/applang-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/applang-gate.sh
# Mutant (dlls/windows.globalization/app_languages.c): SG_MUTANT_NO_APP_LANGUAGES.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-applang.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/applang-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
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
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'statics=00000000' "ApplicationLanguages' statics"
chk 'languages=1 first_is_ui=1' "Languages: the user's UI languages"
chk 'manifest=1' "ManifestLanguages"
chk 'override=00000000,fr-FR,fr-FR' "PrimaryLanguageOverride kept and first in Languages"
chk 'bad_tag=80070057' "a string that is no language tag is refused"
chk 'cleared=1,1' "an empty one clears it"
chk 'for_user=1' "GetLanguagesForUser"
chk 'done=1' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
