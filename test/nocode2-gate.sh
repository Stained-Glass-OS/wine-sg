#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Exports that had no code, batch 2 (patches/sg/8015-8016; test/nocode2-probe.c). Spec stubs before:
# rasdlg RasDialDlgA/W (mRemoteNG's delay import) dial a phone-book entry with its stored credentials,
# the RAS error in dwError; newdev DiUninstallDevice (Chromium's tests import it) removes a device and its
# children through DIF_REMOVE, administrators only, flags 0.
#   WINE=/opt/wine-sg/bin/wine test/nocode2-gate.sh
# Mutants: SG_MUTANT_NO_RASDIALDLG (rasdlg), SG_MUTANT_NO_DIUNINSTALL (newdev).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-nocode2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/nocode2-probe.c" \
    -lrasapi32 -lsetupapi -ladvapi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
cd "$T" || exit 1
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
