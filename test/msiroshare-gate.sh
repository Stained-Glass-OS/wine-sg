#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package an install has open can still be opened read-only (wine-sg 7726). msi keeps the cached
# package open for writing (transacted) while it installs, and MsiOpenDatabase(MSIDBOPEN_READONLY) asked
# to keep writers out, so it failed with ERROR_FUNCTION_FAILED; Java 8's installer.exe, run by its MSI,
# opens the cached package that way and then crashed (the install hung on its crash dialog). Now a
# read-only open that meets a writer reads a snapshot. 64- and 32-bit; test/msiroshare-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/msiroshare-gate.sh
# Mutant: SG_MUTANT_MSI_RO_DENYWRITE (dlls/msi/database.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msiroshare.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/msiroshare-probe.c" -lmsi &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/msiroshare-probe.c" -lmsi ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
