#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfmediaengine's stubbed IMFMediaEngine/IMFMediaEngineEx members (patches/sg/2870), on Xvfb:
# test/mfme-probe.c drives an idle engine, an engine with a generated AVI file
# and a shut-down engine, and the gate fails when a formerly stubbed function
# logs a FIXME on a path that is now answered.
#
#   WINE=/opt/wine-sg/bin/wine test/mfme-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mfmediaengine, -DSG_MUTANT_x): ME_BALANCE_RANGE, ME_STREAMSEL, ME_TIMELINE, ME_S3D, ME_SHUTDOWN, ME_RATE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfme.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfme-probe.exe" "$HERE/mfme-probe.c" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfme-probe.exe" "$WINEPREFIX/drive_c/"
# the loaded-source checks need a media file Wine can demux: the 64x64 I420 AVI
# of the conformance tests (SG_TEST_AVI, or found next to a build tree)
for f in "${SG_TEST_AVI:-/nonexistent}" "$(dirname "$WINE")/../wine-10.0/dlls/mfmediaengine/tests/i420-64x64.avi"; do
    [ -f "$f" ] && { cp "$f" "$WINEPREFIX/drive_c/clip.avi"; break; }
done
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfme-probe.exe 'C:\\clip.avi' 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:mfplat:(media_engine_(SetSourceElements|Load|IsSeeking|GetStartTime|GetPlayed|SetAutoPlay|SetLoop|GetStatistics|UpdateVideoStream|GetBalance|SetBalance|IsPlaybackRateSupported|GetStreamSelection|SetStreamSelection|ApplyStreamSelections|IsProtected|SetTimelineMarkerTimer|GetTimelineMarkerTimer|CancelTimelineMarkerTimer|IsStereo3D|GetStereo3D|SetStereo3D|EnableWindowlessSwapchainMode|EnableHorizontalMirrorMode|EnableTimeUpdateTimer|gs_GetService)|classfactory_LockServer)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
