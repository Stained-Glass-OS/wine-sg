#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"

# windows.media.speech, patch 2810, on Xvfb: test/wmspeech-probe.c asks every
# factory and object the DLL hands out for GetIids, GetTrustLevel,
# GetRuntimeClassName and an unknown IID (these were FIXME stubs returning
# E_NOTIMPL), then the real members: IAsyncInfo::Id, the Completed handler
# kept after it ran, Cancel on a closed operation, the list constraint's
# Tag/Type/Probability/IsEnabled, the session's AutoStopSilenceTimeout,
# StartWithModeAsync/CancelAsync, the recognizer's event registrations and
# DllGetClassObject. The recognizer part SKIPs inside the probe when no audio
# capture device lets the recognizer be created.
#
#   WINE=/opt/wine-sg/bin/wine test/wmspeech-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windows.media.speech, -DSG_MUTANT_x): WMS_IIDS (GetIids drops an
# interface), WMS_TRUST (PartialTrust), WMS_ASYNC_ID (Id is 0), WMS_HANDLER
# (Completed forgets the handler once run), WMS_CONSTRAINT (wrong Type),
# WMS_TIMEOUT (a negative timeout is accepted).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmspeech.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmspeech-probe.exe" "$HERE/wmspeech-probe.c" -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmspeech-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmspeech-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
