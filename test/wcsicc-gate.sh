#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WcsCreateIccProfile (patches/sg/7716): an ICC profile from a color profile -- a copy, since
# Wine's profiles are ICC already. Qt asks for it for each screen; the unimplemented stub was an
# exception in its window procedure, Qt then had "no screens available", and GpgFrontend (Qt 6,
# MSIX) showed no window (2026-11-08).
#   WINE=/opt/wine-sg/bin/wine test/wcsicc-gate.sh
# Mutant: SG_MUTANT_NO_WCS_ICC (dlls/mscms/profile.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wcsicc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
trap '"$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/wcsicc-probe.c" -lmscms || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=1"; then echo "PASS  $2"; else echo "FAIL  $2: $(printf '%s' "$out" | tr '\n' ' ')"; RC=1; fi
}
expect Source "WcsOpenColorProfile opens the system's sRGB profile"
expect Created "WcsCreateIccProfile makes a profile of it"
expect Icc "an ICC profile ('acsp')"
expect Same "with the source's bytes"
expect Independent "still there once the source is closed"
expect Closed "and closed like any profile"
expect Bad "a handle that is no profile: NULL"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
