#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dinput's device properties and device types (patches/sg/2875), on Xvfb:
# test/dinputtodo-probe.c checks the system keyboard and mouse (names, types,
# firmware revisions, axis mode, user name, type name, app data) and, with a
# virtual gamepad from test/wgi-uinput-pad.py (045e:028e), the HID joystick's
# joystick id, axis mode, type name and legacy device type. Without write
# access to /dev/uinput the pad checks are skipped with a note. The log is also
# checked: IDirectInputEffect::Escape must not log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dinputtodo-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dinput, -DSG_MUTANT_x): DIT_MOUSE_TRADITIONAL, DIT_FWREV, DIT_AXISMODE, DIT_USERNAME,
# DIT_APPDATA, DIT_JOYID, DIT_TYPENAME, DIT_CALIBRATION.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dinputtodo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
PAD=
cleanup() { [ -n "$PAD" ] && kill "$PAD" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/dinputtodo-probe.exe" "$HERE/dinputtodo-probe.c" -ldinput8 -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
# the virtual pad must exist before wineboot starts the bus
if [ -w /dev/uinput ] && command -v python3 >/dev/null; then
    python3 "$HERE/wgi-uinput-pad.py" > "$T/pad.out" 2>&1 &
    PAD=$!
    i=0
    until grep -q created "$T/pad.out" 2>/dev/null; do
        i=$((i + 1)); [ $i -gt 40 ] && { echo "note  could not create the uinput gamepad"; kill $PAD 2>/dev/null; PAD=; break; }
        sleep 0.25
    done
    sleep 1
else
    echo "note  /dev/uinput is not writable: no virtual gamepad"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dinputtodo-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" dinputtodo-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:[a-z]+:(hid_joystick_effect_Escape|dinput_device_Escape)' "$T/stderr.log"; then
    echo "FAIL  Escape logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
