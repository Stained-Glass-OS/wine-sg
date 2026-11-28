#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Scanning through WIA (patches/sg/8300-8306). On Windows the WIA device managers
# (CLSID_WiaDevMgr, CLSID_WiaDevMgr2) live in the WIA service (stisvc, wiaservc.dll in
# svchost) and programs reach them out of process: NAPS2's WIA driver asks for
# CLSCTX_LOCAL_SERVER and got "class not registered" (0x80040154) for WIA 2; WIA 1 listed no
# device. Now the service lists SANE's scanners (an imaging class ID each), builds the item
# tree (WIA 1: Root and its image item; WIA 2: Root, Flatbed, Feeder), keeps their
# properties (ranges, lists, checked writes, dependent values) and scans: WIA 1 banded
# transfers (a DIB) and idtGetData into the program's file, WIA 2 IWiaTransfer::Download into
# the program's streams, a page per stream from the feeder. Every interface crosses the
# process boundary (proxies in sti.dll, also of IEnumSTATPROPSTG). The gate uses SANE's
# "test" devices through a SANE configuration of its own (SANE_CONFIG_DIR: no other
# backend, nothing on the network), with 64- and 32-bit probes.
#
#   WINE=/opt/wine-sg/bin/wine test/wiascan-gate.sh   (GATE_INC=<dir>: the probe's WIA headers)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
SANE_TEST=
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib64/sane /usr/lib/sane; do [ -e "$d/libsane-test.so.1" ] && SANE_TEST=$d; done
[ -n "$SANE_TEST" ] || { echo "SKIP: no SANE test backend (libsane1)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-wiascan.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${GATE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0

# the probe's headers: Wine's WIA headers (the mingw ones are WIA 1 of Windows XP)
W=$(dirname "$WINE")
mkdir -p "$T/inc"
for f in wia_lh.h wiadef.h; do
    for d in ${GATE_INC:-} "$W/include" "$W/../include/wine/windows" "$W/../wine-10.0/include" /opt/wine-sg/include/wine/windows; do
        [ -f "$d/$f" ] && { cp "$d/$f" "$T/inc/"; break; }
    done
    [ -f "$T/inc/$f" ] || { echo "SKIP: no $f of this Wine"; exit 77; }
done
for m in "$MINGW:64" "$MINGW32:32"; do
    TMPDIR=/var/tmp "${m%:*}" -O2 -I"$T/inc" -o "$T/probe${m#*:}.exe" "$HERE/wiascan-probe.c" -lole32 -loleaut32 -luuid \
        || { echo "FAIL  the probe did not build"; exit 1; }
done

# SANE with the test backend alone
mkdir -p "$T/sane"
echo test > "$T/sane/dll.conf"
cat > "$T/sane/test.conf" <<'EOC'
number_of_devices 2
mode Color
depth 8
hand-scanner false
three-pass false
three-pass-order RGB
resolution_min 1.0
resolution_max 1200.0
resolution_quant 1.0
resolution 50.0
test-picture "Color pattern"
invert-endianess false
read-limit false
read-limit-size 1
read-delay false
read-delay-duration 1000
read-status-code "Default"
fuzzy-parameters false
ppl-loss 0
non-blocking false
select-fd false
enable-test-options false
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
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1
for b in 64 32; do
    for m in list scan1 file1 feeder1 scan2 feeder2 props; do
        # the WIA service starts with the first call (SANE_CONFIG_DIR comes along); a fresh one each time
        timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" $m >"$T/raw" 2>/dev/null </dev/null
        tr -d '\r' <"$T/raw" >"$T/probe$b-$m.out"
        "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
        sed 's/^/      /' "$T/probe$b-$m.out"
        case $m in
        list) what="the WIA 1 and 2 device managers list the scanners from the service" ;;
        scan1) what="WIA 1: properties of the image item and a banded scan (DIB)" ;;
        file1) what="WIA 1: idtGetData scans into the program's PNG file" ;;
        feeder1) what="WIA 1: the feeder gives a page per transfer, then says it is empty" ;;
        scan2) what="WIA 2: the Flatbed item scans into the program's stream (BMP)" ;;
        feeder2) what="WIA 2: the Feeder gives a stream a page until it is empty" ;;
        props) what="property ranges, lists, checked writes and the property enumerator" ;;
        esac
        if grep -q '^RESULT: PASS' "$T/probe$b-$m.out" && ! grep -q '^FAIL' "$T/probe$b-$m.out"; then
            printf 'PASS  %s-bit: %s\n' $b "$what"
        else printf 'FAIL  %s-bit: %s\n' $b "$what"; RC=1; fi
    done
done
cd /
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
