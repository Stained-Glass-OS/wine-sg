#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A CUPS printer's Windows driver is named after its make and model
# (patches/sg/0871). Wine named every CUPS queue's driver after the queue
# ("DYMO_LabelWriter_550"); Windows names a printer's driver after the model
# ("DYMO LabelWriter 550"), and programs recognise their printers by it --
# DYMO Connect lists only printers whose driver is a LabelWriter's. The gate
# runs a CUPS server of its own (no root, no printer: a queue to /dev/null
# with a PPD for the model) and lists Wine's printers, 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/cupsmodel-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-cupsmodel.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
cat > "$T/root/cupsd.conf" <<EOF
Listen $T/cups.sock
LogLevel warn
<Location />
Order allow,deny
Allow all
</Location>
EOF
cat > "$T/root/cups-files.conf" <<EOF
ServerRoot $T/root
CacheDir $T/cache
StateDir $T/state
RequestRoot $T/spool
TempDir $T/spool
ErrorLog $T/log/error_log
AccessLog $T/log/access_log
PageLog $T/log/page_log
ServerBin /usr/lib/cups
DataDir /usr/share/cups
User $(id -un)
Group $(id -gn)
EOF
cat > "$T/root/printers.conf" <<EOF
<Printer DYMO_LabelWriter_550>
Info Label printer
MakeModel DYMO LabelWriter 550
DeviceURI file:///dev/null
State Idle
Accepting Yes
</Printer>
EOF
cat > "$T/root/ppd/DYMO_LabelWriter_550.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGLW550.PPD"
*Manufacturer: "DYMO"
*Product: "(DYMO LabelWriter 550)"
*ModelName: "DYMO LabelWriter 550"
*ShortNickName: "DYMO LabelWriter 550"
*NickName: "DYMO LabelWriter 550"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: False
*DefaultColorSpace: Gray
*FileSystem: False
*Throughput: "1"
*LandscapeOrientation: Plus90
*TTRasterizer: Type42
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 10 AnySetup *PageSize
*DefaultPageSize: w72h154
*PageSize w72h154/30252 Address: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
*DefaultPageRegion: w72h154
*PageRegion w72h154/30252 Address: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: w72h154
*ImageableArea w72h154/30252 Address: "4 4 68 150"
*DefaultPaperDimension: w72h154
*PaperDimension w72h154/30252 Address: "72 154"
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
lpstat -v 2>/dev/null | grep -q DYMO_LabelWriter_550 || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/wmiprinter-probe.c" -lwinspool -lole32 -loleaut32 -luuid -lwbemuuid ||
        { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
for b in 64 32; do
    out=$(run "C:\\probe$b.exe" enum)
    printf '      %s\n' "$out"
    if printf '%s\n' "$out" | grep -qx 'enum DYMO_LabelWriter_550|DYMO LabelWriter 550|CUPS:DYMO_LabelWriter_550'; then
        pass "$b-bit: the CUPS queue's driver is its model, \"DYMO LabelWriter 550\""
    else
        fail "$b-bit: the CUPS queue's driver is not named after its model"
    fi
done
# the model-named driver has the queue's PPD: the printer's paper is the label
out=$(run 'C:\probe64.exe' papers DYMO_LabelWriter_550)
printf '%s\n' "$out" | grep -qx 'paper 30252 Address' && pass "the driver carries the queue's PPD (paper: 30252 Address)" ||
    fail "the model-named driver lost the queue's PPD ($out)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
