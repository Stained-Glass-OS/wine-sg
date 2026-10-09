#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The class factories' LockServer of qdvd, xaudio2_7 and evr (patches/sg/2837),
# on Xvfb: test/lockserver-probe.c locks and unlocks each factory (S_OK) and
# the gate fails when any logs a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/lockserver-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (evr, -DSG_MUTANT_x): LOCKSERVER_FIXME (the FIXME is back).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-lockserver.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/lockserver-probe.exe" "$HERE/lockserver-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/lockserver-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" lockserver-probe.exe 2>"$T/stderr.log" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E 'fixme:[a-z0-9_]+:(class_factory_LockServer|xapocf_LockServer|XAudio2CF_LockServer|classfactory_LockServer)' "$T/stderr.log"; then
    echo "FAIL  a LockServer logged a FIXME"
    exit 1
fi
echo "PASS  no LockServer logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
