#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The app and system modes a new user starts with (patch 7735): HKCU\...\Themes\Personalize
# AppsUseLightTheme 1 and SystemUsesLightTheme 0, as Windows writes them for a new user. Without the values
# WPF's ThemeMode=System (.NET 9+) takes the user as dark: LanguageTool for Desktop drew its dark-theme
# (white) text on its light content cards. A value the user set is kept on a prefix update.
#   WINE=/opt/wine-sg/bin/wine test/applightmode-gate.sh
# No #define is possible in wine.inf: a build without the two lines fails the first two checks.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-applightmode.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
unset WAYLAND_DISPLAY
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
K='HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
val() { timeout -s KILL 60 "$WINE" reg query "$K" /v "$1" 2>/dev/null | tr -d '\r' | awk -v n="$1" '$1 == n { print $3 }'; }
RC=0
a=$(val AppsUseLightTheme); s=$(val SystemUsesLightTheme)
echo "      new prefix: AppsUseLightTheme=$a SystemUsesLightTheme=$s"
if [ "$a" = "0x1" ]; then echo "PASS  a new user's apps are light (AppsUseLightTheme 1)"; else echo "FAIL  AppsUseLightTheme is '$a', want 0x1"; RC=1; fi
if [ "$s" = "0x0" ]; then echo "PASS  a new user's system mode is dark (SystemUsesLightTheme 0)"; else echo "FAIL  SystemUsesLightTheme is '$s', want 0x0"; RC=1; fi
timeout -s KILL 60 "$WINE" reg add "$K" /v AppsUseLightTheme /t REG_DWORD /d 0 /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add "$K" /v SystemUsesLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1
"$WINESERVER" -w
a=$(val AppsUseLightTheme); s=$(val SystemUsesLightTheme)
echo "      after the user's choice and a prefix update: AppsUseLightTheme=$a SystemUsesLightTheme=$s"
if [ "$a" = "0x0" ] && [ "$s" = "0x1" ]; then echo "PASS  the user's own modes survive a prefix update"
else echo "FAIL  a prefix update overwrote the user's modes ($a, $s)"; RC=1; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
