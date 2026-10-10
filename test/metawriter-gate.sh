#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Metadata writers (patches/sg/2614): test/metawriter-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/metawriter-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windowscodecs): SG_MUTANT_MDW_SETVALUE_APPENDS (metadatahandler.c, SetValue
# never replaces), SG_MUTANT_MDW_REMOVE_STATUS (a missing item is removed with S_OK),
# SG_MUTANT_MDW_NOCACHE_KEEPS (NoCacheStream is not honoured), SG_MUTANT_MDW_NULL_CLEARS
# (LoadEx without a stream empties the handler), SG_MUTANT_MDW_VENDOR_ZERO (the preferred
# vendor is not kept), SG_MUTANT_MDW_FACTORY_OPTIONS (imgfactory.c, persist options are
# accepted for a new writer), SG_MUTANT_MDW_NESTED_PLAIN (imgfactory.c, a writer made
# from a reader keeps its nested readers), SG_MUTANT_MDW_IFD_NO_NESTED (metadatahandler.c,
# the IFD loader leaves the Exif/GPS pointers as numbers), SG_MUTANT_MDW_DIRTY_STUCK
# (IsDirty never says dirty).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-metawriter.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/metawriter-probe.exe" "$HERE/metawriter-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -lpropsys -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/metawriter-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
