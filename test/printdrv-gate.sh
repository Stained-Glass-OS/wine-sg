#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Printing through Windows printer drivers (patches/sg/0920-0925).
#
# Wine printed only through its own PostScript driver; a printer maker's
# Windows driver could not be installed or used. The gate installs two
# driver packages of our own from INF files in the driver store, as a
# maker's installer leaves them:
#   - "SG Test UMPD": a user-mode printer graphics driver (printdrv-umpd.c),
#     the kind most makers ship (Zebra, Brother, Epson);
#   - "SG Test Unidrv": a GPD printer on our Unidrv with a render plug-in
#     (printdrv-plugin.c), the kind DYMO's LabelWriter 550 driver is; and
#     "SG Test Unidrv Raw", a GPD printer without one.
# Each printer prints to a file port. A program's page (a black box at
# 20,10-120,60) must come out of the driver: 64-bit in process, and 32-bit
# through the 64-bit print host (the packages are 64-bit only, as on a
# 64-bit Windows).
#
#   WINE=/opt/wine-sg/bin/wine test/printdrv-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-printdrv.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

# the engine services a driver imports from gdi32, as a maker's driver links them
printf 'LIBRARY gdi32.dll\nEXPORTS\nEngCreateBitmap\nEngAssociateSurface\nEngDeleteSurface\nEngWritePrinter\n' > "$T/eng.def"
"${MINGW%-gcc}-dlltool" -d "$T/eng.def" -l "$T/libeng.a" || { echo "SKIP: no dlltool"; exit 77; }
"$MINGW" -shared -O2 -o "$T/sgtestumpd.dll" "$HERE/printdrv-umpd.c" "$T/libeng.a" -lgdi32 -lwinspool || { echo "FAIL  driver did not build"; exit 1; }
"$MINGW" -shared -O2 -o "$T/sgtestplug.dll" "$HERE/printdrv-plugin.c" -lole32 -luuid || { echo "FAIL  plug-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$T/probe32.exe" "$C/"

# the packages, as an installer leaves them in the driver store
S="$C/windows/system32/DriverStore/FileRepository"
mkdir -p "$S/sgtestumpd.inf_1/amd64" "$S/sgtestuni.inf_1/amd64"
cp "$T/sgtestumpd.dll" "$S/sgtestumpd.inf_1/amd64/"
printf 'SG test data\r\n' > "$S/sgtestumpd.inf_1/sgtest.dat"
cat > "$S/sgtestumpd.inf_1/sgtestumpd.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Test UMPD" = SGTD_INSTALL, SGTEST_UMPD

[SGTD_INSTALL]
CopyFiles=@sgtestumpd.dll, @sgtest.dat
DriverFile=sgtestumpd.dll
ConfigFile=sgtestumpd.dll
DataFile=sgtest.dat

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,

[SourceDisksFiles]
sgtestumpd.dll = 1
sgtest.dat = 2
EOF
cp "$T/sgtestplug.dll" "$S/sgtestuni.inf_1/amd64/"
printf '[OEMFiles]\r\nOEMDriverFile1=sgtestplug.dll\r\n' > "$S/sgtestuni.inf_1/sgtestuni.ini"
cat > "$S/sgtestuni.inf_1/sgtest.gpd" <<'EOF'
*GPDSpecVersion: "1.0"
*% our own GPD for the gate
*GPDFileName: "sgtest.gpd"
*ModelName: "SG Test Unidrv"
*MasterUnits: PAIR(100, 100)
*PrinterType: SERIAL
*Feature: Orientation
{
    *DefaultOption: PORTRAIT
    *Option: PORTRAIT { *Name: "Portrait" }
    *Option: LANDSCAPE_CC90 { *Name: "Landscape" }
}
*Feature: Resolution
{
    *DefaultOption: R100
    *Option: R100
    {
        *Name: "100 dpi"
        *DPI: PAIR(100, 100)
        EXTERN_GLOBAL: *StripBlanks: LIST(TRAILING)
    }
}
*Feature: PaperSize
{
    *DefaultOption: L2x1
    *Option: L2x1
    {
        *Name: "SG Label 2 x 1"
        *OptionID: 300
        *PageDimensions: PAIR(200, 100)
        *PrintableArea: PAIR(200, 100)
        *PrintableOrigin: PAIR(0, 0)
        *Command: CmdSelect
        {
            *Order: DOC_SETUP.5
            *Cmd: "<1B>P1"
        }
    }
    *Option: L3x1
    {
        *Name: "SG Label 3 x 1"
        *OptionID: 258
        *PageDimensions: PAIR(300, 100)
        *PrintableArea: PAIR(300, 100)
        *PrintableOrigin: PAIR(0, 0)
        *Command: CmdSelect { *Order: DOC_SETUP.5 *Cmd: "<1B>P2" }
    }
    *Option: L4x1
    {
        *Name: "SG Label 4 x 1"
        *PageDimensions: PAIR(400, 100)
        *PrintableArea: PAIR(400, 100)
        *PrintableOrigin: PAIR(0, 0)
        *Command: CmdSelect { *Order: DOC_SETUP.5 *Cmd: "<1B>P3" }
    }
}
*Feature: ColorMode
{
    *DefaultOption: Enhanced
    *Option: Enhanced
    {
        *Name: "Enhanced"
        *DevBPP: 1
        *DevNumOfPlanes: 1
        *DrvBPP: 24
        *IPCallbackID: 100
    }
}
*Command: CmdStartDoc
{
    *Order: JOB_SETUP.1
    *Cmd: "SGU START<0A>"
}
*Command: CmdStartPage
{
    *Order: PAGE_SETUP.1
    *Cmd: "SGU PAGE<0A>"
}
*Command: CmdEndJob
{
    *Order: JOB_FINISH.1
    *Cmd: "SGU END<0A>"
}
*Command: CmdFF { *Cmd : "" }
EOF
cat > "$S/sgtestuni.inf_1/sgtestraw.gpd" <<'EOF'
*GPDSpecVersion: "1.0"
*GPDFileName: "sgtestraw.gpd"
*ModelName: "SG Test Unidrv Raw"
*MasterUnits: PAIR(100, 100)
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
        *Name: "SG Label 2 x 1"
        *PageDimensions: PAIR(200, 100)
        *PrintableArea: PAIR(200, 100)
        *PrintableOrigin: PAIR(0, 0)
    }
}
*Feature: ColorMode
{
    *DefaultOption: Mono
    *Option: Mono { *Name: "Mono" *DevBPP: 1 *DevNumOfPlanes: 1 *DrvBPP: 1 }
}
*StripBlanks: LIST(TRAILING)
*Command: CmdYMoveAbsolute { *Cmd : "<1B>*p" %d{DestY}"Y" }
*Command: CmdSendBlockData { *Cmd : "<1B>*b" %d{NumOfDataBytes}"W" }
*Command: CmdFF { *Cmd : "<0C>" }
EOF
cat > "$S/sgtestuni.inf_1/sgtestuni.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64.6.3

[SGTEST.NTamd64.6.3]
"SG Test Unidrv" = SGU_INSTALL, SGTEST_UNI
"SG Test Unidrv Raw" = SGR_INSTALL, SGTEST_RAW

[SGU_INSTALL]
CopyFiles=@sgtest.gpd, SGU_PLUGIN
DataFile=sgtest.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[SGR_INSTALL]
CopyFiles=@sgtestraw.gpd
DataFile=sgtestraw.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[SGU_PLUGIN]
sgtestplug.dll
sgtestuni.ini

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,

[SourceDisksFiles]
sgtestplug.dll = 1
sgtestuni.ini = 2
sgtest.gpd = 2
sgtestraw.gpd = 2
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
for m in "SG Test UMPD" "SG Test Unidrv" "SG Test Unidrv Raw"; do
    out=$(run 'C:\probe64.exe' install "$m")
    [ "$out" = "install 0" ] && pass "\"$m\" installs from its INF" || fail "\"$m\" did not install: $out"
done
"$WINESERVER" -w
for p in LPT1 LPT2 LPT3; do
    timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "$p:" /d "$T/$p.out" /f >/dev/null 2>&1
done
run 'C:\probe64.exe' add "UMPD Printer" "SG Test UMPD" "LPT1:" >/dev/null
run 'C:\probe64.exe' add "Unidrv Printer" "SG Test Unidrv" "LPT2:" >/dev/null
run 'C:\probe64.exe' add "Unidrv Raw" "SG Test Unidrv Raw" "LPT3:" >/dev/null

# the program's DC measures the driver's device
for b in 64 32; do
    out=$(run "C:\\probe$b.exe" caps "UMPD Printer")
    [ "$out" = "caps 200 100 100 210 5" ] && pass "$b-bit: the DC measures the driver's device ($out)" ||
        fail "$b-bit: the DC does not measure the driver's device: $out"
done
out=$(run 'C:\probe64.exe' papers "UMPD Printer")
[ "$out" = "paper 256 533x279 SG Test 2x1" ] && pass "the driver's configuration DLL answers DeviceCapabilities" ||
    fail "DeviceCapabilities: $out"
out=$(run 'C:\probe64.exe' papers "Unidrv Printer" | tr '\n' ';')
# a paper without an OptionID gets one no other paper has (lw5xx.gpd's
# seventh paper met its 262)
[ "$out" = "paper 300 508x254 SG Label 2 x 1;paper 258 762x254 SG Label 3 x 1;paper 261 1016x254 SG Label 4 x 1;" ] &&
    pass "Unidrv lists the GPD's papers, each with its own id" || fail "Unidrv papers: $out"

expect_umpd='SGTD START printdrv probe
PAGE 1 200x100 dark=5000 box=20,10-119,59
SGTD END'
for b in 64 32; do
    rm -f "$T/LPT1.out" "$T/LPT2.out" "$T/LPT3.out"
    run "C:\\probe$b.exe" print "UMPD Printer" >/dev/null
    out=$(tr -d '\r' < "$T/LPT1.out" 2>/dev/null)
    [ "$out" = "$expect_umpd" ] && pass "$b-bit: the page came out of the maker-style driver" ||
        { fail "$b-bit: the driver's output is not the page"; printf '      %s\n' "$out"; }

    run "C:\\probe$b.exe" print "Unidrv Printer" >/dev/null
    out=$(tr -d '\r' < "$T/LPT2.out" 2>/dev/null)
    head=$(printf '%s\n' "$out" | head -4 | tr '\n' ';')
    rows=$(printf '%s\n' "$out" | grep -c '^ROW [0-9]* 100$')
    first=$(printf '%s\n' "$out" | grep -m1 '^ROW')
    last=$(printf '%s\n' "$out" | tail -1)
    if [ "$head" = "HOOK START;SGU START;$(printf '\033')P1SGU PAGE;IP 224x100 id 100 dark 5000;" ] && [ "$rows" = 50 ] &&
       [ "$first" = "ROW 10 100" ] && [ "$last" = "SGU END" ]; then
        pass "$b-bit: Unidrv ran the GPD's commands and the plug-in's hook, band (padded with paper) and rows (as ink)"
    else
        fail "$b-bit: Unidrv with the plug-in: head '$head', $rows rows, first '$first', last '$last'"
    fi

    run "C:\\probe$b.exe" print "Unidrv Raw" >/dev/null
    blocks=$(grep -a -o "$(printf '\033')\*b15W" "$T/LPT3.out" 2>/dev/null | wc -l)
    firsty=$(grep -a -o "$(printf '\033')\*p[0-9]*Y" "$T/LPT3.out" 2>/dev/null | head -1 | tr -d '\033')
    [ "$blocks" = 50 ] && [ "$firsty" = "*p10Y" ] && pass "$b-bit: Unidrv sent the GPD's raster commands (50 rows from row 10)" ||
        fail "$b-bit: Unidrv raster: $blocks blocks, first move '$firsty'"
done

[ $RC = 0 ] && echo "printdrv gate: PASS" || echo "printdrv gate: FAIL"
exit $RC
