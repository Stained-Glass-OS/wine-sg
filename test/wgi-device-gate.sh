#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.gaming.input's device-bound stubs (patches/sg/2800), on Xvfb with a
# virtual gamepad: test/wgi-uinput-pad.py makes a uinput device (045e:028e),
# test/wgi-device-probe.c registers a custom factory for that hardware id and
# checks what the DLL reports for it (provider, RawGameController and
# IGameController members that were FIXME stubs).
#
#   WINE=/opt/wine-sg/bin/wine test/wgi-device-gate.sh
# SKIPs (77) without write access to /dev/uinput, or when the new input node is
# not readable by the user (udev normally grants the logged-in seat).
# Mutants (windows.gaming.input, -DSG_MUTANT_x): WGI_REGISTER_IGNORED (the
# registered factory is never given the device).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -w /dev/uinput ] || { echo "SKIP: /dev/uinput is not writable"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgidevice.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
PAD=
cleanup() { [ -n "$PAD" ] && kill "$PAD" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wgi-device-probe.exe" "$HERE/wgi-device-probe.c" -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgi-device-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 200 "$WINE" wgi-device-probe.exe 2>/dev/null </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
# the probe registers its factory and prints READY; only then is the pad plugged in
( timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh" ) &
RUN=$!
i=0
until grep -qx READY "$T/probe.out" 2>/dev/null; do
    i=$((i + 1)); [ $i -gt 600 ] && { echo "FAIL  the probe never became ready"; cat "$T/probe.out" 2>/dev/null; exit 1; }
    sleep 0.5
done
python3 "$HERE/wgi-uinput-pad.py" > "$T/pad.out" 2>&1 &
PAD=$!
i=0
until grep -q created "$T/pad.out" 2>/dev/null; do
    i=$((i + 1)); [ $i -gt 40 ] && { echo "SKIP: could not create the uinput gamepad"; cat "$T/pad.out"; exit 77; }
    sleep 0.25
done
sleep 1
NODE=$(grep -l 'SG Test Pad' /sys/class/input/event*/device/name 2>/dev/null | head -1 | sed 's|.*/\(event[0-9]*\)/.*|/dev/input/\1|')
[ -n "$NODE" ] && [ -r "$NODE" ] || { echo "SKIP: the new input node ${NODE:-?} is not readable"; exit 77; }
wait $RUN
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
