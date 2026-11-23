#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI's voices are usable from keys the caller may only read (patches/sg/8700).
# sapi opened the voice category, its Tokens key and each token for writing (KEY_ALL_ACCESS).
# The voices live below HKEY_LOCAL_MACHINE, which a standard user -- and an administrator
# who is not elevated -- may only read on Stained Glass: SpObjectTokenCategory::SetId gave
# SPERR_INVALID_REGISTRY_KEY, SpVoice found no voice and Speak failed, so nothing read text
# aloud (Thorium's read aloud stopped at its first sentence, qa4 2026-11-22). Now a key that
# refuses writing is opened for reading, as SAPI does on Windows; writing to it still fails.
# The probe builds a category with a read-only DACL under HKEY_CURRENT_USER, so the gate does
# not depend on who runs it.
#   WINE=/opt/wine-sg/bin/wine test/sapiro-gate.sh
# Mutant: SG_MUTANT_SAPI_NO_READONLY_KEYS (dlls/sapi/token.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapiro.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/sapiro-probe.c" -lole32 -ladvapi32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
OUT=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
"$WINESERVER" -w
printf '%s\n' "$OUT" | sed 's/^/  /'
case "$OUT" in
*"SETUP "*) echo "FAIL  the probe could not make a read-only key"; exit 1;;
*"RESULT PASS"*) echo "PASS  sapiro: voice category and tokens open from a read-only key"; exit 0;;
*) echo "FAIL  sapiro"; exit 1;;
esac
