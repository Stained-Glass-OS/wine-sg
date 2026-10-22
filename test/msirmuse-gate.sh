#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msi.dll (patch 5952): a file in use while its product is uninstalled is deleted at the next restart
# (PendingFileRenameOperations, the removal returns 3010) and its emptied folder after it, as with
# Windows Installer; the restart (wineboot) then removes both. Mutant SG_MUTANT_MSIRMUSE: kept forever.
#
#   WINE=/opt/wine-sg/bin/wine test/msirmuse-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msirmuse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/msirmuse-probe.exe" "$HERE/msirmuse-probe.c" -lmsi -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/msirmuse-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1
timeout -s KILL 180 "$WINE" msirmuse-probe.exe remove 2>/dev/null </dev/null | tr -d '\r' > "$T/remove.out"
"$WINESERVER" -w
# the restart: wineboot carries out the pending operations
timeout -s KILL 180 "$WINE" wineboot >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 180 "$WINE" msirmuse-probe.exe after 2>/dev/null </dev/null | tr -d '\r' > "$T/after.out"
cat "$T/remove.out" "$T/after.out"
grep -q '^RESULT: PASS' "$T/remove.out" && grep -q '^RESULT: PASS' "$T/after.out" || exit 1
exit 0
