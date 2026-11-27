#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmvcore writer with a profile (patches/sg/2865): SetProfile, SetOutputFilename, input count/formats/props, SetInputProps, the unconfigured writer's state errors, BeginWriting checks (2026-11-27: since 6517 the writer writes and since 9261 a script stream too, so BeginWriting gets past the streams and may fail only for a missing encoder; its 'no encoder' FIXME is allowed).
# Stderr is checked: none of the implemented functions may log a FIXME.
# Mutants (wmvcore, -DSG_MUTANT_x): WMVW_NOFORMATCHECK, WMVW_NOOUTPUTCHECK, WMVW_NOCHOSEN, WMVW_NO_SCRIPT (9261)
#
#   WINE=/opt/wine-sg/bin/wine test/wmvw-writer-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmvw-writer.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+wmvcore WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmvw-writer-probe.exe" "$HERE/wmvw-writer-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmvw-writer-probe.exe" "$WINEPREFIX/drive_c/"

cat > "$T/run.sh" <<EOR
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmvw-writer-probe.exe  2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOR
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# allowed: 'no encoder' (an encoder this machine lacks) and the image stream's 'not written'
# (WMMEDIATYPE_Image {34a50fd8-...}: ASF image streams are not written)
if grep -E 'fixme:wmvcore:' "$T/stderr.log" | grep -Ev 'no encoder|major type \{34a50fd8-8aa5-4386-81fe-a0efe0488e31\}.* not written'; then
    echo "FAIL  a function logged a FIXME"
    exit 1
fi
echo "PASS  no FIXME logged"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo "FAIL  the probe did not run to the end (crash)"
exit 1
