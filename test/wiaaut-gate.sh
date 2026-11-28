#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WIA Automation (wiaaut.dll, patches/sg/8300-8306): the scripting objects of WIA, as VBScript,
# PowerShell and .NET programs (Paint.NET's "From Scanner or Camera") use them --
# WIA.DeviceManager, DeviceInfo/Device/Item with their Properties (by name and by number,
# ranges, read-only ones), Item.Transfer into an ImageFile (size, format, resolution, SaveFile
# that does not replace a file), WIA.ImageProcess filters (RotateFlip, Convert), Vector,
# and WIA.CommonDialog without its dialogs (one scanner: no Select Device). Wine had no
# wiaaut.dll at all. The script runs late bound through cscript against SANE's "test"
# device (one), through the WIA service.
#
#   WINE=/opt/wine-sg/bin/wine test/wiaaut-gate.sh

set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
SANE_TEST=
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib64/sane /usr/lib/sane; do [ -e "$d/libsane-test.so.1" ] && SANE_TEST=$d; done
[ -n "$SANE_TEST" ] || { echo "SKIP: no SANE test backend (libsane1)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-wiaaut.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${GATE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0

mkdir -p "$T/sane"
echo test > "$T/sane/dll.conf"
cat > "$T/sane/test.conf" <<'EOC'
number_of_devices 1
mode Color
depth 8
resolution 50.0
test-picture "Color pattern"
geometry_min 0.0
geometry_max 200.0
geometry_quant 1.0
tl_x 0.0
tl_y 0.0
br_x 80.0
br_y 100.0
EOC
export SANE_CONFIG_DIR="$T/sane"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$HERE/wiaaut-script.vbs" "$WINEPREFIX/drive_c/script.vbs"
for b in 64 32; do
    host="$WINEPREFIX/drive_c/windows/system32/cscript.exe"
    [ $b = 32 ] && host="$WINEPREFIX/drive_c/windows/syswow64/cscript.exe"
    rm -rf "$WINEPREFIX/drive_c/out$b"; mkdir -p "$WINEPREFIX/drive_c/out$b"
    (cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" "$host" //nologo "C:\\script.vbs" "C:\\result$b.txt" \
        "C:\\out$b" >/dev/null 2>&1 </dev/null)
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
    tr -d '\r' <"$WINEPREFIX/drive_c/result$b.txt" >"$T/result$b" 2>/dev/null
    sed 's/^/      /' "$T/result$b"
    if grep -q '^RESULT: PASS' "$T/result$b" && ! grep -q '^FAIL' "$T/result$b"; then
        echo "PASS  $b-bit: WIA Automation lists, connects, sets properties, scans, filters and saves"
    else echo "FAIL  $b-bit: WIA Automation lists, connects, sets properties, scans, filters and saves"; RC=1; fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
