#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Unidrv raster for colour laser drivers (patches/sg/1023), built as
# Lexmark's universal driver is: a 24-bit device (red, green, blue), the
# raster commands defined in the Resolution option, raster groups
# (CmdSetSrcBmpWidth, CmdBeginRaster, rows, CmdEndRaster) compressed with
# TIFF 4.0 when the GPD can switch it on, an HP-GL/2 personality and a
# plug-in (unidrvgl-plugin.c) that hooks only DrvBitBlt and hands the
# drawing back to the core. The gate reads the PCL raster back and finds
# the program's red box, red, where it was drawn.
#   WINE=/opt/wine-sg/bin/wine test/unidrvrast-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-unidrvrast.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
# on a failure, what Wine said (a gate that fails only now and then)
diagnose() { [ $RC = 0 ] && return; echo "--- wineboot: rc $BOOT_RC, ${BOOT_TIME}s; drive_c: $(ls "$C" 2>&1 | tr '\n' ' ')"; tail -3 "$T/wineboot.log"; echo "--- printers:"; run 'C:\probe64.exe' enum 2>/dev/null; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme\|wayland" | tail -15; }

"$MINGW" -shared -O2 -o "$T/sgglplug.dll" "$HERE/unidrvgl-plugin.c" -lole32 -luuid || { echo "FAIL  plug-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
BOOT_START=$(date +%s)
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; BOOT_RC=$?; "$WINESERVER" -w
BOOT_TIME=$(( $(date +%s) - BOOT_START ))
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sggl.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgglplug.dll" "$S/amd64/"
printf '[OEMFiles]\r\nOEMDriverFile1=sgglplug.dll\r\n' > "$S/sggl.ini"
cat > "$S/sggl.gpd" <<'EOF'
*GPDSpecVersion: "1.0"
*GPDFileName: "sggl.gpd"
*ModelName: "SG GL Color"
*MasterUnits: PAIR(100, 100)
*Personality: "HPGL2"
*Feature: Resolution
{
    *DefaultOption: R100
    *Option: R100
    {
        *Name: "100 dpi"
        *DPI: PAIR(100, 100)
        *Command: CmdBeginRaster { *Cmd: "<1B>*r1A" }
        *Command: CmdEndRaster { *Cmd: "<1B>*rB" }
        *Command: CmdSendBlockData { *Cmd: "<1B>*b" %d{NumOfDataBytes}"W" }
    }
}
*Feature: PaperSize
{
    *DefaultOption: L2x1
    *Option: L2x1
    {
        *Name: "SG GL 2 x 1"
        *PageDimensions: PAIR(200, 100)
        *PrintableArea: PAIR(200, 100)
        *PrintableOrigin: PAIR(0, 0)
    }
}
*Feature: ColorMode
{
    *DefaultOption: Color
    *Option: Color
    {
        *Name: "Color"
        *DevNumOfPlanes: 1
        *DevBPP: 24
        *DrvBPP: 24
        *Color?: TRUE
        *Command: CmdSetSrcBmpWidth { *Cmd: "<1B>*r" %d{RasterDataWidthInBytes / 3}"S" }
    }
}
*Command: CmdEnableTIFF4 { *Cmd: "<1B>*b2M" }
*Command: CmdDisableCompression { *Cmd: "<1B>*b0M" }
*Command: CmdXMoveAbsolute { *Cmd: "<1B>*p" %d{DestX}"X" }
*Command: CmdYMoveAbsolute { *Cmd: "<1B>*p" %d{DestY}"Y" }
*Command: CmdFF { *Cmd: "<0C>" }
EOF
cat > "$S/sggl.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG GL Color" = SGGL_INSTALL

[SGGL_INSTALL]
CopyFiles=@sggl.gpd, @sgglplug.dll, @sggl.ini
DataFile=sggl.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,

[SourceDisksFiles]
sgglplug.dll = 1
sggl.ini = 2
sggl.gpd = 2
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\probe64.exe' install "SG GL Color")
[ "$out" = "install 0" ] && pass "the package installs" || fail "install: $out"
"$WINESERVER" -w
timeout 120 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$T/LPT1.out" /f >/dev/null 2>&1
run 'C:\probe64.exe' add "GL Printer" "SG GL Color" "LPT1:" >/dev/null
run 'C:\probe64.exe' print "GL Printer" "" ff0000 >/dev/null
res=$(python3 - "$T/LPT1.out" <<'EOF'
import re, sys
d = open(sys.argv[1], 'rb').read()
if not d.startswith(b'GLBLT\n'):
    print('no plug-in blit', d[:20]); sys.exit()
width = None; mode = 0; y = 0; rows = {}; groups = 0
i = 0
cmd = re.compile(rb'\x1b\*([a-z])(-?\d*)([A-Za-z])')
while True:
    m = cmd.search(d, i)
    if not m: break
    g, num, term = m.group(1), m.group(2), m.group(3)
    i = m.end()
    if g == b'r' and term == b'S': width = int(num)
    elif g == b'r' and term == b'A': groups += 1
    elif g == b'p' and term == b'Y': y = int(num)
    elif g == b'b' and term == b'M': mode = int(num)
    elif g == b'b' and term == b'W':
        n = int(num); data = d[i:i + n]; i += n
        if mode == 2:
            out = bytearray(); j = 0
            while j < len(data):
                c = data[j]; j += 1
                if c < 128: out += data[j:j + c + 1]; j += c + 1
                elif c > 128: out += bytes([data[j]]) * (257 - c); j += 1
            data = bytes(out)
        rows[y] = data; y += 1
red = 0; box = [999, 999, -1, -1]; other = 0
for ry, data in rows.items():
    for x in range(len(data) // 3):
        p = data[x * 3:x * 3 + 3]
        if p == b'\xff\xff\xff': continue
        if p == b'\xff\x00\x00':
            red += 1
            box = [min(box[0], x), min(box[1], ry), max(box[2], x), max(box[3], ry)]
        else: other += 1
print('width=%s mode=%d groups=%d red=%d other=%d box=%d,%d-%d,%d' % (width, mode, groups, red, other, *box))
EOF
)
[ "$res" = "width=200 mode=2 groups=1 red=5000 other=0 box=20,10-119,59" ] &&
    pass "the plug-in's blit came back to the core: one raster group of red, green and blue rows, TIFF 4.0, the red box where it was drawn" ||
    fail "the raster: $res"

diagnose
[ $RC = 0 ] && echo "unidrvrast gate: PASS" || echo "unidrvrast gate: FAIL"
exit $RC
