#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dinput's IDirectInputJoyConfig8 methods and force feedback effect files (patches/sg/2876..2879),
# on Xvfb: test/dinputleft-probe.c checks the cooperative level / acquire state machine, argument
# validation, the OEM type / per-id configuration / user value round trips, and effect file
# write+enumerate round trips (what the interface documentation fixes only).  The log must
# not contain a FIXME from dinput.
#
#   WINE=/opt/wine-sg/bin/wine test/dinputleft-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dinput8, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_DIL_* hooks in dinput.c / device.c.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dinputleft.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/dinputleft-probe.exe" "$HERE/dinputleft-probe.c" -ldinput8 -ldxguid -lole32 -luuid -luser32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dinputleft-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 "$WINE" dinputleft-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:dinput:' "$T/stderr.log"; then
    echo "FAIL  dinput logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
