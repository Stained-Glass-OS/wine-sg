#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mf's sequencer source (IMFSequencerSource + IMFMediaSourceEx, patches/sg/2973-2974)
# and the standard quality manager's notifications (2975): test/mf-sequencer-probe.c
# checks values from the public documentation. No audio device or display needed;
# also fails when a formerly stubbed function logs a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/mf-sequencer-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mf, -DSG_MUTANT_x): SEQ_APPEND_AFTER_LAST, SEQ_FLAGS_IGNORED, SEQ_STOP_IDLE_OK, SEQ_UPDATE_NOOP, SEQ_ID_SAME, QM_NOSHUTDOWN.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfseq.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mf-sequencer-probe.exe" "$HERE/mf-sequencer-probe.c" -lmf -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mf-sequencer-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mf-sequencer-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"; [ -n "${SG_GATE_VERBOSE:-}" ] && tail -20 "$T/stderr.log"
if grep -E 'fixme:[a-z]+:(seq_source_|standard_quality_manager_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
