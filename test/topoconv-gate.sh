#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A media session plays uncompressed video the video renderer cannot take as it is
# (patches/sg/9414).
#
# The topology loader puts the video processor between such a stream and the EVR
# and offers the renderer the processor's output types. They came bare (subtype,
# frame size): the EVR's mixer took each for a compressed type and refused it, and
# the loader failed with MF_E_TOPO_CODEC_NOT_FOUND -- no media session played WMV,
# MPEG-1/2, MPEG-4 part 2 or uncompressed AVI video (the source hands those out
# decoded, as I420 or RGB). The types now carry the processor's input type's
# attributes, as the source reader's pipeline gives them.
#
# The probe (test/topoconv-probe.c) writes an RGB24 AVI, resolves source -> EVR
# with the topology loader (resolved, one converter, its output uncompressed),
# then plays it in a media session to MESessionEnded.
#   WINE=/opt/wine-sg/bin/wine test/topoconv-gate.sh
# Mutant: SG_MUTANT_TOPO_BARE_OUTPUT_TYPES (mf topology_loader.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-topoconv.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/topoconv-probe.c" -lole32 -luuid -lmfplat -lmf -lmfuuid -luser32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
timeout -s KILL 120 "$WINE" 'C:\probe.exe' >"$T/out" 2>/dev/null
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check write        "the probe wrote its AVI"
check source       "the media source opens it"
check load         "the topology loader resolves source -> EVR"
check converter    "one converter between them"
check uncompressed "the converter's output type is uncompressed"
check played       "a media session plays it to the end"
exit $RC
