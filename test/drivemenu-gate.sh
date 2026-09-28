#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A drive's menu in File Explorer: Open first, Eject for a disc or a
# removable drive, no Cut or Delete (patches/sg/0468).
#
# A drive item had no file type, so its menu's first, default item came from
# whatever the buffer held ("shell"); it offered Cut and Delete, and no Eject
# -- a disc in a drive could not be ejected from File Explorer (the mounted
# disc keeps the tray locked). Eject runs sg-session's sg-eject (unmount, then
# eject, through UDisks2) and falls back to IOCTL_STORAGE_EJECT_MEDIA.
#
#   WINE=/opt/wine-sg/bin/wine test/drivemenu-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-drivemenu.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/drivemenu-probe.exe" "$HERE/drivemenu-probe.c" -lshell32 -lole32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# D: a disc in a drive, E: a fixed disk -- as mountmgr's Drives setting makes them
mkdir -p "$T/disc" "$T/disk"; echo x > "$T/disc/README.TXT"
ln -s "$T/disc" "$WINEPREFIX/dosdevices/d:"; ln -s "$T/disk" "$WINEPREFIX/dosdevices/e:"
"$WINE" reg add 'HKLM\Software\Wine\Drives' /v d: /d cdrom /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Wine\Drives' /v e: /d hd /f >/dev/null 2>&1
"$WINESERVER" -k; "$WINESERVER" -w
cd="$("$WINE" "$T/drivemenu-probe.exe" D 2>/dev/null | tr -d '\r')"
hd="$("$WINE" "$T/drivemenu-probe.exe" E 2>/dev/null | tr -d '\r')"
echo "      D: $(echo "$cd" | tr '\n' '|')"; echo "      E: $(echo "$hd" | tr '\n' '|')"
[ "$(echo "$cd" | head -1)" = "*&Open" ] || [ "$(echo "$cd" | head -1)" = "*Open" ] \
    && pass "a drive's first, default item is Open" || fail "first item: $(echo "$cd" | head -1)"
echo "$cd" | grep -qx 'Eje&ct' && pass "a disc drive offers Eject" || fail "no Eject on the disc drive"
echo "$hd" | grep -qx 'Eje&ct' && fail "a fixed disk offers Eject" || pass "a fixed disk does not"
echo "$cd$hd" | grep -qx 'Cu&t\|&Delete' && fail "a drive offers Cut or Delete" || pass "no drive offers Cut or Delete"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
