#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll: kernel32 is usable as soon as it is in the module list (patches/sg/8291; test/k32early-probe.c).
# Firefox's launcher hooks NtMapViewOfSection in its suspended browser process and calls kernel32's
# GetModuleHandleW from the hook; Wine mapped kernel32 before kernelbase, the hook found kernel32 with its
# imports not bound yet, and Firefox's first start (and every start in launcher mode) crashed.  The probe
# hooks its own suspended child the same way.  The module lists keep kernel32 before kernelbase.
#   WINE=/opt/wine-sg/bin/wine test/k32early-gate.sh
# Mutant: SG_MUTANT_KERNEL32_FIRST (dlls/ntdll/loader.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-k32early.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -fno-stack-protector -o "$T/probe.exe" "$HERE/k32early-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 90 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
line=$(grep '^child ' "$T/out")
v() { printf '%s\n' "$line" | sed -n "s/.* $1=\([0-9-]*\).*/\1/p"; }
fail=0
[ "$(v hookcalls)" -gt 0 ] 2>/dev/null && echo "PASS  the hook ran in the child ($(v hookcalls) mappings)" || { echo "FAIL  the hook did not run"; fail=1; }
[ "$(v k32calls)" -gt 0 ] 2>/dev/null && [ "$(v k32bad)" = 0 ] && echo "PASS  kernel32's GetModuleHandleW worked from the hook ($(v k32calls) calls)" || { echo "FAIL  kernel32 from the hook: calls '$(v k32calls)', wrong '$(v k32bad)'"; fail=1; }
[ "$(v order)" = 1 ] && echo "PASS  the module lists keep kernel32 before kernelbase" || { echo "FAIL  module list order"; fail=1; }
grep -qx 'RESULT: PASS' "$T/out" && [ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
