#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.ui: scheduled toasts persist and are delivered after the program has exited
#
#   WINE=/opt/wine-sg/bin/wine test/fastwinui-sched-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastwinui-sched.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sched-probe.exe" "$HERE/fastwinui-sched-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sched-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { timeout -s KILL 60 "$WINE" sched-probe.exe "\$@" >"$T/p.out" 2>/dev/null </dev/null; tr -d '\r' <"$T/p.out"; }
Q() { timeout -s KILL 60 "$WINE" reg query "\$1" /s 2>/dev/null </dev/null | tr -d '\r'; }
H='HKCU\\Software\\Stained Glass\\Notifications\\History'
S='HKCU\\Software\\Stained Glass\\Notifications\\Scheduled'
{
echo "add: \$(P add 5)"
echo "list-new-process: \$(P list | grep COUNT)"
Q 'HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run' | grep -q SgToastScheduler && echo "RUNKEY yes" || echo "RUNKEY no"
i=0
while [ \$i -lt 30 ]; do
    Q "\$H" | grep -q "persisted toast 5" && break
    i=\$((i + 1)); sleep 2
done
Q "\$H" | grep -q "persisted toast 5" && echo "DELIVERED yes" || echo "DELIVERED no"
echo "list-after: \$(P list | grep COUNT)"
Q "\$S" | grep -q Aumid && echo "ENTRY left" || echo "ENTRY gone"
echo "add2: \$(P add 300)"
echo "remove: \$(P remove | grep -v RESULT | tr '\n' ' ')"
Q "\$S" | grep -q Aumid && echo "ENTRY2 left" || echo "ENTRY2 gone"
} > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^list-new-process: COUNT 1' "$T/probe.out" && grep -q '^DELIVERED yes' "$T/probe.out" &&
grep -q '^ENTRY gone' "$T/probe.out" && grep -q '^list-after: COUNT 0' "$T/probe.out" &&
grep -q '^RUNKEY yes' "$T/probe.out" && grep -q '^remove: COUNT 1 COUNT 0' "$T/probe.out" &&
grep -q '^ENTRY2 gone' "$T/probe.out" && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
