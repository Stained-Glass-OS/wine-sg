#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Scanning through TWAIN (patches/sg/9815, 9817, 9818): a Windows program -- IrfanView's
# Acquire, NAPS2's TWAIN driver -- opens the source manager, picks a source
# (sane.ds: one per SANE device), scans a page without the source's own window
# and takes it as a DIB (native transfer). TWAIN 2 passes that DIB's handle
# pointer-sized; sane.ds wrote only 32 bits of it, so a 64-bit program got a
# cut or half-garbage handle (a GlobalAlloc pointer above 4 GB). The gate uses
# SANE's "test" device through a SANE configuration of its own
# (SANE_CONFIG_DIR: no other backend, nothing on the network), 64- and
# 32-bit, under Xvfb (the source shows its progress window). 9817: strips of a
# memory transfer, one page from the flatbed, interleaved RGB, the resolution
# asked for. 9818: the source's own window (its option pages; it crashed or
# came up empty) and TWAIN 2 through twaindsm.dll (64-bit programs load
# TWAINDSM.DLL: IrfanView 64-bit), entry points and callbacks.
#
#   WINE=/opt/wine-sg/bin/wine test/twainscan-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
SANE_TEST=
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib64/sane /usr/lib/sane; do [ -e "$d/libsane-test.so.1" ] && SANE_TEST=$d; done
[ -n "$SANE_TEST" ] || { echo "SKIP: no SANE test backend (libsane1)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-twainscan.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0

# SANE with the test backend alone
mkdir -p "$T/sane"
echo test > "$T/sane/dll.conf"
# the test device's settings (without them it makes 1x1 frames and never ends a page)
cat > "$T/sane/test.conf" <<'EOC'
number_of_devices 2
mode Gray
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
for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -O2 -o "$T/probe${m#*:}.exe" "$HERE/twainscan-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
for b in 64 32; do
    for m in plain dsm ui; do
        a=; [ \$m = plain ] || a=\$m
        timeout -s KILL 120 "$WINE" "C:\\\\probe\$b.exe" \$a 2>/dev/null </dev/null | tr -d '\r' > "$T/probe\$b-\$m.out"
    done
done
EOS
chmod +x "$T/run.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
for b in 64 32; do
    for m in plain dsm ui; do
        sed 's/^/      /' "$T/probe$b-$m.out"
        case $m in
        plain) what="a page scanned through TWAIN arrives as a DIB and in strips" ;;
        dsm) what="a TWAIN 2 program scans through twaindsm.dll, its events by callback" ;;
        ui) what="the source's own window has its pages and scans" ;;
        esac
        if grep -q '^RESULT: PASS' "$T/probe$b-$m.out"; then printf 'PASS  %s-bit: %s\n' $b "$what"
        else printf 'FAIL  %s-bit: %s\n' $b "$what"; RC=1; fi
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
