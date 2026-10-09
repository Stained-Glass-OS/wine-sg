#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dsound's FIXME stubs (patches/sg/2833), on Xvfb: test/dsound-stubs-probe.c
# calls Restore (primary and secondary), AcquireResources, the capture
# buffer's Initialize / GetObjectInPath / GetFXStatus, LockServer and
# IKsPropertySet::Set and checks the results. The run's log is also checked:
# none of those may log a FIXME any more.
#
#   WINE=/opt/wine-sg/bin/wine test/dsound-stubs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dsound, -DSG_MUTANT_x): DSOUND_FIXME (Restore logs a FIXME again),
# DSOUND_CAPTURE (Initialize / GetFXStatus succeed), DSOUND_ACQUIRE (no result codes).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dsoundstubs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dsound WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/dsound-stubs-probe.exe" "$HERE/dsound-stubs-probe.c" -ldsound -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dsound-stubs-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" dsound-stubs-probe.exe 2>"$T/stderr.log" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E 'fixme:dsound:(PrimaryBufferImpl_Restore|IDirectSoundBufferImpl_Restore|IDirectSoundBufferImpl_AcquireResources|IDirectSoundCaptureBufferImpl_(Initialize|GetObjectInPath|GetFXStatus)|DSCF_LockServer|IKsPrivatePropertySetImpl_Set)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
