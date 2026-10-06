#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Unidrv for makers' PCL XL drivers (patches/sg/1024), such as HP's universal
# PCL 6 driver: a GPD whose commands are built of value macros from
# included files (=NAME tokens, a % written in hex as <2525>), more than
# 128 features, *Personality PCLXL, and a render plug-in that writes the
# printer language itself from the blits it hooks (unidrvxl-plugin.c).
#   - the plug-in is handed the page through its DrvCopyBits hook, and
#     Unidrv sends no raster of its own for it;
#   - the plug-in's devmode part is sized with the driver object, so it can
#     ask the core about the printer;
#   - a PCL XL GPD without such a plug-in gets the page as one raster block
#     (CmdBeginRaster, CmdSendBlockData, CmdEndRaster), rows padded to DWORDs.
#   WINE=/opt/wine-sg/bin/wine test/unidrvxl-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-unidrvxl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

"$MINGW" -shared -O2 -o "$T/sgxlplug.dll" "$HERE/unidrvxl-plugin.c" -lole32 -luuid || { echo "FAIL  plug-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sgxl.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgxlplug.dll" "$S/amd64/"
printf '[OEMFiles]\r\nOEMDriverFile1=sgxlplug.dll\r\n' > "$S/sgxl.ini"
cat > "$S/sgxlmac.gpd" <<'EOF'
*% value macros, as a maker's include files hold them
*Macros: SGMACROS
{
    SGHEAD: "SGXL " =SGWORD
    SGWORD: "START<0A>"
    SGPCT: "<2525>"
}
EOF
features() {
    i=1
    while [ $i -le 140 ]; do
        printf '*Feature: F%03d\n{\n    *DefaultOption: On\n    *Option: On { *Name: "On" }\n}\n' $i
        i=$((i + 1))
    done
}
common() {
    cat <<'EOF'
*MasterUnits: PAIR(100, 100)
*Include: "sgxlmac.gpd"
*Personality: "PCLXL"
*Feature: Resolution
{
    *DefaultOption: R100
    *Option: R100 { *Name: "100 dpi" *DPI: PAIR(100, 100) }
}
*Feature: PaperSize
{
    *DefaultOption: L2x1
    *Option: L2x1
    {
        *Name: "SG XL 2 x 1"
        *PageDimensions: PAIR(200, 100)
        *PrintableArea: PAIR(200, 100)
        *PrintableOrigin: PAIR(0, 0)
    }
}
*Command: CmdStartDoc { *Order: DOC_SETUP.1 *Cmd: =SGHEAD "pct=" =SGPCT "<0A>" }
*Command: CmdEndJob { *Order: JOB_FINISH.1 *Cmd: "SGXL END<0A>" }
*Command: CmdFF { *Cmd : "" }
EOF
    features
    cat <<'EOF'
*Feature: SGLate
{
    *DefaultOption: Yes
    *Option: Yes
    {
        *Name: "Yes"
        *Command: CmdSelect { *Order: DOC_SETUP.9 *Cmd: "LATE<0A>" }
    }
}
EOF
}
{ printf '*GPDSpecVersion: "1.0"\n*GPDFileName: "sgxl.gpd"\n*ModelName: "SG XL Plugin"\n'; common
  cat <<'EOF'
*Feature: ColorMode
{
    *DefaultOption: Color
    *Option: Color { *Name: "Color" *DevBPP: 24 *DevNumOfPlanes: 1 *DrvBPP: 24 }
}
EOF
} > "$S/sgxl.gpd"
{ printf '*GPDSpecVersion: "1.0"\n*GPDFileName: "sgxlr.gpd"\n*ModelName: "SG XL Raster"\n'; common
  cat <<'EOF'
*Feature: ColorMode
{
    *DefaultOption: Mono
    *Option: Mono { *Name: "Mono" *DevBPP: 1 *DevNumOfPlanes: 1 *DrvBPP: 1 }
}
*Command: CmdBeginRaster { *Cmd: "RASTER " %d{RasterDataWidthInBytes} "x" %d{RasterDataHeightInPixels} "<0A>" }
*Command: CmdSendBlockData { *Cmd: "DATA " %d{NumOfDataBytes} "<0A>" }
*Command: CmdEndRaster { *Cmd: "<0A>ENDRASTER<0A>" }
EOF
} > "$S/sgxlr.gpd"
cat > "$S/sgxl.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG XL Plugin" = SGXL_INSTALL
"SG XL Raster" = SGXLR_INSTALL

[SGXL_INSTALL]
CopyFiles=@sgxl.gpd, @sgxlmac.gpd, @sgxlplug.dll, @sgxl.ini
DataFile=sgxl.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[SGXLR_INSTALL]
CopyFiles=@sgxlr.gpd, @sgxlmac.gpd
DataFile=sgxlr.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,

[SourceDisksFiles]
sgxlplug.dll = 1
sgxl.ini = 2
sgxl.gpd = 2
sgxlr.gpd = 2
sgxlmac.gpd = 2
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
for m in "SG XL Plugin" "SG XL Raster"; do
    out=$(run 'C:\probe64.exe' install "$m")
    [ "$out" = "install 0" ] && pass "\"$m\" installs" || fail "\"$m\" did not install: $out"
done
"$WINESERVER" -w
for p in LPT1 LPT2; do
    timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "$p:" /d "$T/$p.out" /f >/dev/null 2>&1
done
run 'C:\probe64.exe' add "XL Plugin" "SG XL Plugin" "LPT1:" >/dev/null
run 'C:\probe64.exe' add "XL Raster" "SG XL Raster" "LPT2:" >/dev/null

run 'C:\probe64.exe' print "XL Plugin" >/dev/null
out=$(tr -d '\r' < "$T/LPT1.out" 2>/dev/null)
expect='SGXL START
pct=%
LATE
XLIMG 200x100 dark=5000 box=20,10-119,59
XLPAGE
SGXL END'
[ "$out" = "$expect" ] && pass "the plug-in got the page through its blit hook; the commands' macros, the escaped % and the 141st feature" ||
    { fail "the vector plug-in's job"; printf '      %s\n' "$out" | head -12; }

run 'C:\probe64.exe' print "XL Raster" >/dev/null
res=$(python3 - "$T/LPT2.out" <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read()
head = b'SGXL START\npct=%\nLATE\n'
if not d.startswith(head):
    print('head', d[:40]); sys.exit()
i = d.find(b'RASTER ')
line = d[i:d.index(b'\n', i)].decode()
j = d.index(b'DATA ', i)
n = int(d[j + 5:d.index(b'\n', j)])
data = d[d.index(b'\n', j) + 1:][:n]
ink = sum(bin(b).count('1') for b in data)
rest = d[d.index(b'\n', j) + 1 + n:]
print(line, n, ink, rest.startswith(b'\nENDRASTER\n'))
EOF
)
[ "$res" = "RASTER 28x100 2800 5000 True" ] && pass "a PCL XL GPD without a drawing plug-in gets the page as one raster block" ||
    fail "the raster block: $res"

[ $RC = 0 ] && echo "unidrvxl gate: PASS" || echo "unidrvxl gate: FAIL"
exit $RC
