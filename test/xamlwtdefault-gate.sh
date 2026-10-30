#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml on by default for Windows Terminal (patches/sg/6192) and OneDrive (6219): wine.inf gives
# HKLM\Software\Wine\AppDefaults\WindowsTerminal.exe SgXaml=1, so a new (or updated) prefix runs
# WindowsTerminal.exe with XAML (6131 keeps it off for other programs); OneDrive.exe likewise (its
# sign-in window is XAML: without it only the tray icon showed).  Windows Terminal has no
# Win32 fallback: with XAML off it stops at its first window.  The probe is test/xamlhost-probe.c,
# run as WindowsTerminal.exe and as another name, without WINE_SG_XAML.
#   WINE=/opt/wine-sg/bin/wine test/xamlwtdefault-gate.sh
# Mutation: drop the SgAppDefaults line(s) from the built wine.inf (the gate fails).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlwtdefault.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/xamlhost-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY WINE_SG_XAML
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
mkdir -p "$WINEPREFIX/drive_c/wt"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/wt/WindowsTerminal.exe"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/wt/other.exe"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/wt/OneDrive.exe"
RC=0
reg=$("$WINE" reg query 'HKLM\Software\Wine\AppDefaults\WindowsTerminal.exe' /v SgXaml 2>/dev/null | tr -d '\r')
printf '%s\n' "$reg" | grep -v '^$' | sed 's/^/      /'
if printf '%s\n' "$reg" | grep -q 'SgXaml *REG_DWORD *0x1$'; then echo "PASS  a new prefix has AppDefaults\\WindowsTerminal.exe SgXaml=1 (HKLM)"
else echo "FAIL  a new prefix has AppDefaults\\WindowsTerminal.exe SgXaml=1 (HKLM)"; RC=1; fi
wt=$(cd "$WINEPREFIX/drive_c/wt" && timeout -s KILL 120 "$WINE" WindowsTerminal.exe 2>/dev/null </dev/null | tr -d '\r' | head -1)
"$WINESERVER" -w
other=$(cd "$WINEPREFIX/drive_c/wt" && timeout -s KILL 120 "$WINE" other.exe 2>/dev/null </dev/null | tr -d '\r' | head -1)
"$WINESERVER" -w
od=$(cd "$WINEPREFIX/drive_c/wt" && timeout -s KILL 120 "$WINE" OneDrive.exe 2>/dev/null </dev/null | tr -d '\r' | head -1)
"$WINESERVER" -w
echo "      WindowsTerminal.exe: $wt"
echo "      OneDrive.exe: $od"
echo "      other.exe: $other"
if [ "$wt" = "manager=00000000" ]; then echo "PASS  XAML is on for WindowsTerminal.exe without WINE_SG_XAML"
else echo "FAIL  XAML is on for WindowsTerminal.exe without WINE_SG_XAML"; RC=1; fi
if [ "$od" = "manager=00000000" ]; then echo "PASS  XAML is on for OneDrive.exe without WINE_SG_XAML"
else echo "FAIL  XAML is on for OneDrive.exe without WINE_SG_XAML"; RC=1; fi
if [ "$other" = "factory Windows.UI.Xaml.Hosting.WindowsXamlManager=80040154" ]; then echo "PASS  and still off for other programs (REGDB_E_CLASSNOTREG)"
else echo "FAIL  and still off for other programs (REGDB_E_CLASSNOTREG)"; RC=1; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
