#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dmsynth's FIXME stubs (patches/sg/2970): test/dmusic-synth-probe.c drives
# IDirectMusicSynth8 directly (channel groups, priorities, running statistics,
# voices, download type check, wave unload callback). The run's log is also
# checked: none of the formerly stubbed functions may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmusic-synth-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dmsynth, -DSG_MUTANT_x): GROUPS (SetNumChannelGroups unchecked),
# PRIORITY (priorities not stored), STATS (no running statistics),
# VOICE (voice state always reports a live voice), DLTYPE (unknown download
# type reaches the offset table check), CALLBACK (wave unload callback never called).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dmusicsynth.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dmsynth WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/dmusic-synth-probe.c" -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
( cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out" )
cat "$T/probe.out"
if grep -E 'fixme:dmsynth:(synth_SetNumChannelGroups|synth_GetRunningStats|synth_SetChannelPriority|synth_GetChannelPriority|synth_PlayVoice|synth_StopVoice|synth_GetVoiceState|synth_Refresh|synth_AssignChannelToBuses|synth_Unload|synth_control_Ks|synth_sink_Activate|synth_sink_control_Ks|latency_clock_|synth_sfont_iter)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
