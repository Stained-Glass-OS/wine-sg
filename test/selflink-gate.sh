#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A shell folder is never linked to itself (patches/sg/0583).
# Stained Glass OS makes a user's Linux home a link to their Windows profile,
# so $HOME/Documents *is* C:\users\<user>\Documents. Asked to link the one to
# the other (winecfg's shell folders; shell32 for a missing folder), the mount
# manager moved the folder aside as Documents.backup and left a link to
# itself: the user's files seemed gone. The probe, with HOME the profile, asks
# for exactly that link: Documents must stay a folder with its file.
#
#   WINE=/opt/wine-sg/bin/wine test/selflink-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-selflink.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/selflink-probe.exe" "$HERE/selflink-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
P="$WINEPREFIX/drive_c/users/$(id -un)"
[ -d "$P" ] || { echo "FAIL  no profile at $P"; exit 1; }
cp "$T/selflink-probe.exe" "$WINEPREFIX/drive_c/"
out=$(HOME="$P" timeout 60 "$WINE" 'C:\selflink-probe.exe' 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
if [ "$out" = selflink=kept ] && [ -d "$P/Documents" ] && [ ! -L "$P/Documents" ]; then
    echo "PASS  the profile's Documents stays a folder when HOME is the profile"; echo "RESULT: PASS"; exit 0
fi
echo "FAIL  Documents was linked to itself ($out; $(ls -ld "$P"/Documents* 2>/dev/null | tr '\n' ' '))"; echo "RESULT: FAIL"; exit 1
