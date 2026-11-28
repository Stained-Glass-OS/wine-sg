#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The WIA dialogs (patches/sg/8300-8306), shown in the calling program as Windows shows them:
# "Select Device" (IWiaDevMgr::SelectDeviceDlg/ID, OK and Cancel), "Scan using <device>"
# (IWiaDevMgr::GetImageDlg: the device, the kind of picture, Scan, the file) and WIA 2's
# "New Scan" (IWiaDevMgr2::GetImageDlg: profile, source, format, file type, Scan, the files).
# Wine answered every dialog call with WIA_S_NO_DEVICE_AVAILABLE. Driven by xdotool on an
# Xvfb of the gate's own, against SANE's "test" devices (two, so Select Device asks).
# GATE_SHOTS=<dir> keeps a picture of each dialog; GATE_INC=<dir> gives the probe its headers.
#
#   WINE=/opt/wine-sg/bin/wine test/wiadlg-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" xdotool; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
SANE_TEST=
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib64/sane /usr/lib/sane; do [ -e "$d/libsane-test.so.1" ] && SANE_TEST=$d; done
[ -n "$SANE_TEST" ] || { echo "SKIP: no SANE test backend (libsane1)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY
sg_gate_display || exit $?
T=$(mktemp -d /var/tmp/sg-wiadlg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${GATE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0

W=$(dirname "$WINE")
mkdir -p "$T/inc"
for f in wia_lh.h wiadef.h; do
    for d in ${GATE_INC:-} "$W/include" "$W/../include/wine/windows" "$W/../wine-10.0/include" /opt/wine-sg/include/wine/windows; do
        [ -f "$d/$f" ] && { cp "$d/$f" "$T/inc/"; break; }
    done
    [ -f "$T/inc/$f" ] || { echo "SKIP: no $f of this Wine"; exit 77; }
done
TMPDIR=/var/tmp "$MINGW" -O2 -I"$T/inc" -o "$T/probe.exe" "$HERE/wiascan-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/sane"
echo test > "$T/sane/dll.conf"
cat > "$T/sane/test.conf" <<'EOC'
number_of_devices 2
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
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"

# wait_window TITLE: the window's id once it is mapped (or nothing after 60 s)
wait_window() {
    i=0
    while [ $i -lt 240 ]; do
        id=$(xdotool search --onlyvisible --name "$1" 2>/dev/null | head -n 1)
        [ -n "$id" ] && { echo "$id"; return 0; }
        sleep 0.25; i=$((i + 1))
    done
    return 1
}

# press WINDOW KEY: the key to the dialog, once it is drawn
press() {
    sleep 1
    xdotool windowfocus --sync "$1" 2>/dev/null
    xdotool key --clearmodifiers "$2" 2>/dev/null
}

shot() {
    [ -n "${GATE_SHOTS:-}" ] && command -v import >/dev/null && mkdir -p "$GATE_SHOTS" && \
        import -window root "$GATE_SHOTS/$1.png" 2>/dev/null
    return 0
}

for m in select cancel scan1 scan2; do
    (cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" "C:\\probe.exe" dlg-$m >"$T/raw-$m" 2>/dev/null </dev/null) &
    P=$!
    ok=1
    case $m in
    select)
        if id=$(wait_window "^Select Device$"); then shot select; press "$id" Return; else ok=0; fi ;;
    cancel)
        if id=$(wait_window "^Select Device$"); then press "$id" Escape; else ok=0; fi ;;
    scan1)
        if id=$(wait_window "^Select Device$"); then press "$id" Return; else ok=0; fi
        if [ $ok = 1 ] && id=$(wait_window "^Scan using "); then
            shot scan1
            press "$id" alt+p; sleep 5; shot scan1-preview
            press "$id" Return
        else ok=0; fi ;;
    scan2)
        if id=$(wait_window "^New Scan$"); then
            shot scan2
            # a preview of the bed, then the scan
            press "$id" alt+p; sleep 5; shot scan2-preview
            press "$id" Return
        else ok=0; fi ;;
    esac
    wait $P
    tr -d '\r' <"$T/raw-$m" >"$T/out-$m"
    sed 's/^/      /' "$T/out-$m"
    case $m in
    select) what="Select Device lists the scanners; OK gives the chosen one" ;;
    cancel) what="Select Device: Cancel is S_FALSE" ;;
    scan1) what="GetImageDlg: Select Device, Scan using <device>, the picture in the file" ;;
    scan2) what="WIA 2 GetImageDlg: New Scan saves the page in the folder" ;;
    esac
    [ $ok = 1 ] || echo "      (the dialog did not come up)"
    if [ $ok = 1 ] && grep -q '^RESULT: PASS' "$T/out-$m" && ! grep -q '^FAIL' "$T/out-$m"; then
        echo "PASS  $what"
    else echo "FAIL  $what"; RC=1; fi
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
