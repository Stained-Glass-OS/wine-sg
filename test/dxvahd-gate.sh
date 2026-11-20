#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DXVA-HD (wine-sg 8207, G-VIDDEC): DXVAHD_CreateDevice was a stub (E_NOTIMPL), so a player that does its
# video processing through DXVA-HD found none. Now a device on the Direct3D 9Ex device: capabilities (one
# processor, one stream), output and input formats, video surfaces (an input format it cannot take
# refused), a video processor, blit states kept and checked by size, and a blit: the stream scaled into its
# destination rectangle, the background colour around it; two streams refused (one is all it has).
# Headless: its own Xvfb, Wine's own Direct3D (wined3d on Mesa).
#   WINE=/opt/wine-sg/bin/wine test/dxvahd-gate.sh
# Mutants (dxva2/dxvahd.c): SG_MUTANT_DXVAHD_STUB, SG_MUTANT_DXVAHD_NO_BACKGROUND, SG_MUTANT_DXVAHD_IGNORE_DEST.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
for need in Xvfb "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dxvahd.XXXXXX)
cleanup() { [ -n "$XP" ] && kill "$XP" 2>/dev/null; WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/dxvahd-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
[ -s "$T/display" ] || { echo "SKIP: Xvfb did not start"; exit 77; }
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;d3d9=b"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then printf 'PASS  %s\n' "$3"
    else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
}
printf '%s\n' "$out" | grep -qx 'd3d=1' || { echo "SKIP: no Direct3D 9Ex device here"; exit 77; }
expect CreateDevice 0 "DXVAHD_CreateDevice gives a device"
expect DeviceCaps "0 1 1 1 1" "its capabilities: output and input formats, one processor, one stream"
expect OutputFormats "0 1" "its output formats (X8R8G8B8 first)"
expect InputFormats "0 1" "its input formats (NV12 first)"
expect ProcessorCaps 0 "the processor's capabilities"
expect OutputSurface 0 "an output video surface"
expect InputSurface 0 "an input video surface"
expect BadFormat 1 "an input format it cannot take: E_INVALIDARG"
expect Processor 0 "a video processor for the processor's GUID"
expect Background "0 1" "the background colour state, set and read back"
expect BadSize 1 "a state of the wrong size: E_INVALIDARG"
expect Destination 0 "the stream's destination rectangle"
expect Blt 0 "VideoProcessBltHD"
expect Inside ff0000 "inside the destination rectangle: the stream (red)"
expect Outside 00ff00 "around it: the background (green)"
expect TwoStreams 1 "two streams: E_INVALIDARG (one is all it has)"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
