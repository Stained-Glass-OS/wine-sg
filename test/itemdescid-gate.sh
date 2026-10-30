#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A shell item's PKEY_DescriptionID (patches/sg/6218): IShellItem2::GetProperty gives its
# SHDESCRIPTIONID as a byte vector (a fixed drive, a folder, a file). It gave S_OK with no value and
# OneDrive's sync service crashed reading it during setup. test/itemdescid-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/itemdescid-gate.sh
# Mutant: SG_MUTANT_NO_ITEM_DESCRIPTION_ID (dlls/shell32/shellitem.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-itemdescid.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/itemdescid-probe.exe" "$HERE/itemdescid-probe.c" -lshell32 -lole32 -luuid -lpropsys || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/itemdescid-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" itemdescid-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^drive=00000000,0x8$' "C:\\ is a fixed drive (SHDID_COMPUTER_FIXED)"
chk '^folder=00000000,0x3$' "a folder (SHDID_FS_DIRECTORY)"
chk '^file=00000000,0x2$' "a file (SHDID_FS_FILE)"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
