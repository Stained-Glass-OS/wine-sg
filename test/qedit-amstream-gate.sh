#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# amstream's seeking filter, multimedia stream and media streams (patches/sg/2992,
# 2993): test/qedit-amstream-probe.c puts a mock source filter (with an
# IMediaSeeking pin) behind the media stream filter and checks its IMediaSeeking
# answers (local ones, forwarded ones, capability checks), the multimedia stream's
# GetInformation / OpenMoniker / Render, audio and DirectDraw streams'
# SetSameFormat, AllocateSample, CreateSharedSample, SendEndOfStream and
# ReceiveMultiple, the samples' SetSampleTimes, SetRect, Update with an event or an
# APC and CompletionStatus (abort, wait). The run's log is also checked: none of
# the formerly stubbed functions may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/qedit-amstream-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (amstream, -DSG_MUTANT_x): AM_RATE, AM_CAPS, AM_AVAILABLE (patch 2992);
# AM_INFO, AM_APC, AM_TIMES, AM_ABORT, AM_SAMEFMT, AM_RECVMULTI, AM_RECT (patch 2993).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qeditam.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/qedit-amstream-probe.exe" "$HERE/qedit-amstream-probe.c" -lole32 -loleaut32 -luuid -lstrmiids -lddraw -ldxguid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/qedit-amstream-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" qedit-amstream-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:(filter_seeking_|multimedia_stream_(GetInformation|OpenMoniker|Render)|(audio|ddraw)_(IAMMediaStream_|sample_(SetSampleTimes|SetRect)|meminput_ReceiveMultiple)|AMCF_LockServer)|APC support is not implemented|Event parameter support is not' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
