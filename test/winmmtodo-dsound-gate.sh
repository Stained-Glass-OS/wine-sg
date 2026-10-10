#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dsound todo_wine groups (patches/sg/2804), on Xvfb: test/winmmtodo-dsound-probe.c
# checks SetFX (format negotiation, result codes, removal on failure), the status of
# a deferred-location buffer and the sub format check. The run's log is also
# checked: no dsound FIXME may be logged.
#
#   WINE=/opt/wine-sg/bin/wine test/winmmtodo-dsound-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dsound, -DSG_MUTANT_x): DSOUND_FX_FLOAT (effects offered a float type),
# DSOUND_FX_KEEP (a failed SetFX keeps the old effects), DSOUND_FX_RESULT (FAILED code
# for a refused type), DSOUND_DEFER_STATUS (LOCDEFER always reports a location).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winmmtodods.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dsound WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/winmmtodo-dsound-probe.exe" "$HERE/winmmtodo-dsound-probe.c" -ldsound -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/winmmtodo-dsound-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" winmmtodo-dsound-probe.exe 2>"$T/stderr.log" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E 'fixme:dsound:' "$T/stderr.log"; then
    echo "FAIL  dsound logged a FIXME"
    exit 1
fi
echo "PASS  dsound logged no FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
