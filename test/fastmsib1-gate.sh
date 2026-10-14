#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msi.dll gap-A batch 1 (patch 3215) and the silent-install checks of 3216 (no window at UILevel NONE/BASIC, probed in the process and on the X server of the gate's own Xvfb): _Validation modes of MsiViewModify + MsiViewGetError list, REPLACE/MERGE/REFRESH/DELETE/INSERT_TEMPORARY on tables, _Streams and _Storages, ValidateProductID templates, MsiGetMode(ZAWENABLED). Mutants SG_MUTANT_MSIB1_*.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmsib1-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastmsib1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastmsib1-probe.exe" "$HERE/fastmsib1-probe.c" -lmsi -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastmsib1-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
# sampler: the probe writes the name of its phase to C:\\phase.txt; this loop records the phase and how many
# windows the X server shows (mapped, with a name) every 0.1 s
( while :; do echo "\$(cat "$WINEPREFIX/drive_c/phase.txt" 2>/dev/null)|\$(xdotool search --onlyvisible --name . 2>/dev/null | wc -l)"; sleep 0.1; done ) > "$T/x11.log" 2>/dev/null &
SAMPLER=\$!
timeout -s KILL 180 "$WINE" fastmsib1-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
kill \$SAMPLER 2>/dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
grep -v '^# ' "$T/probe.out"
rc=1
grep -q '^RESULT: PASS' "$T/probe.out" && rc=0
# the X server must have shown nothing during the NONE and BASIC phases, and something during FULL ones
if command -v xdotool >/dev/null; then
    bad=$(awk -F'|' '$1 ~ /-(NONE|BASIC)$/ && $2 > 0' "$T/x11.log" | sort | uniq -c | head -3)
    full=$(awk -F'|' '$1 ~ /-FULL$/ && $2 > 0' "$T/x11.log" | wc -l)
    n=$(wc -l < "$T/x11.log")
    if [ -n "$bad" ]; then echo "FAIL  the X server showed windows in a silent phase:"; echo "$bad"; rc=1; fi
    if [ "$full" -lt 3 ]; then echo "FAIL  the X sampler saw no window in the FULL phases ($full of $n samples): the check proves nothing"; rc=1; fi
    echo "x11 sampler: $n samples, $full with windows in FULL phases"
else
    echo "NOTE: xdotool missing, X server window check skipped"
fi
exit $rc
