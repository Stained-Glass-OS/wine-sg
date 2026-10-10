#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplay's IMFPMediaPlayer members that were stubs (patches/sg/2871), on Xvfb:
# test/mfplay-probe.c drives a player with no item, with a media item (the
# conformance tests' AVI, see below), and after Shutdown, plus DllGetClassObject;
# the gate fails when a formerly stubbed function logs a FIXME on a path that
# is now answered.
#
#   WINE=/opt/wine-sg/bin/wine test/mfplay-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   SG_TEST_MEDIA=file for the item checks away from a build tree
# Mutants (mfplay, -DSG_MUTANT_x): MFP_VOLUME_RANGE, MFP_BALANCE, MFP_MUTE, MFP_EFFECT_DUP, MFP_SHUTDOWN, MFP_POSITION.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfplay.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfplay-probe.exe" "$HERE/mfplay-probe.c" -lmfplay -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfplay-probe.exe" "$WINEPREFIX/drive_c/"
# the loaded-source checks need a media file Wine can demux: the AVI file
# of the conformance tests (SG_TEST_MEDIA, or found next to a build tree)
for f in "${SG_TEST_MEDIA:-/nonexistent}" "$(dirname "$WINE")/../wine-10.0/dlls/mfmediaengine/tests/i420-64x64.avi"; do
    [ -f "$f" ] && { cp "$f" "$WINEPREFIX/drive_c/clip.avi"; break; }
done
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfplay-probe.exe 'C:\\clip.avi' 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:mfplay:media_player_(SetPosition|GetVolume|SetVolume|GetBalance|SetBalance|GetMute|SetMute|InsertEffect|RemoveEffect|RemoveAllEffects)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
