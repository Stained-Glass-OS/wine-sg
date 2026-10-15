#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Patch 3609: hidclass' device thread no longer leaves a read IRP pointing at its own stack. At session
# shutdown (services get SERVICE_CONTROL_SHUTDOWN since 2223) winedevice removes its devices; the bus driver
# completes the pending IOCTL_HID_READ_REPORT after the hid thread is gone and wrote through the dead stack
# (Unhandled page fault in ntoskrnl IoCompleteRequest, 15-20% of short runs). The gate starts and ends the
# session LOOPS times (default 40) and counts the faults.
#   WINE=/path/to/build/wine test/fastb-hidstack-gate.sh      LOOPS=40
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
LOOPS="${LOOPS:-40}"
T=$(mktemp -d /var/tmp/sg-fastb-hidstack.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
n=0; bad=0
while [ \$n -lt $LOOPS ]; do
  out=\$(timeout -s KILL 90 "$WINE" cmd /c exit 2>&1 </dev/null)
  echo "\$out" | grep -q "Unhandled page fault" && bad=\$((bad+1))
  "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
  n=\$((n+1))
done
echo "faults \$bad of $LOOPS" > "$T/result.txt"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 1500 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/result.txt"
grep -q '^faults 0 ' "$T/result.txt" && { echo "PASS  no winedevice page fault"; exit 0; }
echo "FAIL  winedevice page faults at shutdown"; exit 1
