#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The filter graph asks a filter's pin enumerator for a count (patches/sg/9108; AviSynth's
# DirectShowSource refuses a NULL pcFetched, so Wine's graph saw its filter as pinless and
# RenderFile built a renderer of its own): RenderFile, Connect, RenderStream and FindPin with such a
# filter in the graph (test/strictpins-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/strictpins-gate.sh
# Mutants: SG_MUTANT_STRICTPINS_NULL (quartz filtergraph.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-strictpins.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/strictpins-probe.c" -lole32 -loleaut32 -luuid -lstrmiids || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check wav                "a PCM WAV file"
check renderfile         "RenderFile ends at the filter already in the graph"
check no_extra_renderer  "no audio renderer of the graph's own"
check renderfile_counts  "RenderFile always asks for the count"
check connect            "Connect reaches the filter"
check renderstream       "RenderStream reaches the filter"
check findpin            "FindPin finds its pin"
check builder_counts     "the capture graph builder always asks for the count"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
