#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 (patch 5955): SHGetKnownFolderIDList(KF_FLAG_NO_ALIAS) gives the Desktop and Documents folders
# on disk; without the flag the namespace aliases are unchanged. ImgBurn recursed forever at start
# without it (qa4/qa5 2026-10-22). Mutant SG_MUTANT_NOALIAS (dlls/shell32/shellpath.c).
#
#   WINE=/opt/wine-sg/bin/wine test/noalias-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-noalias.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/noalias-probe.exe" "$HERE/noalias-probe.c" -lshell32 -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/noalias-probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
cat "$T/out"
grep -q '^RESULT: PASS' "$T/out"
