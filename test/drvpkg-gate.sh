#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Printer makers' driver packages as they ship (patches/sg/1020-1022, 1032).
#
# Our own package, laid out as makers lay theirs out, must install and
# print through Wine's spooler as on Windows:
#   - the driver's files named in a DataSection, a file installed under
#     another name ("destination,source"), a file compressed for setup
#     (sgmaker.da_), a file inside the source disk's cabinet, a file for
#     the system directory (DestinationDirs 66002);
#   - a model an undecorated (32-bit Windows) list names is not installed
#     on 64-bit when the package carries its own code, is when it is all
#     data on the core drivers (old makers' PPD packages), and the
#     decorated list wins when both name a model;
#   - a package carrying its own copies of the core drivers (UNIDRV.DLL)
#     gets ours;
#   - the driver (drvpkg-umpd.c) draws on a surface of its own, takes the
#     page through DrvCopyBits and EngCopyBits, reads its data file with
#     EngLoadModule, writes with WritePrinter on its printer handle, and its
#     configuration DLL reads the printer through the handle it is given;
#     it keeps a setting of its own behind the public ones and looks its job
#     up when the document starts, and asks the engine for a code page's
#     glyph set (EngComputeGlyphSet, 1033).  A 32-bit program gets its
#     papers and its settings through our 64-bit print host, and prints
#     with them (1032). It hears GDI's document events (1035).
#
#   WINE=/opt/wine-sg/bin/wine test/drvpkg-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-drvpkg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
# on a failure, what Wine said (a gate that fails only now and then)
diagnose() { [ $RC = 0 ] && return; echo "--- wineboot: rc $BOOT_RC, ${BOOT_TIME}s; drive_c: $(ls "$C" 2>&1 | tr '\n' ' ')"; tail -3 "$T/wineboot.log"; echo "--- printers:"; run 'C:\probe64.exe' enum 2>/dev/null; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme\|wayland" | tail -15; }

printf 'LIBRARY gdi32.dll\nEXPORTS\nEngCreateBitmap\nEngCreateDeviceSurface\nEngAssociateSurface\nEngDeleteSurface\nEngLockSurface\nEngUnlockSurface\nEngCopyBits\nEngCreatePalette\nEngDeletePalette\nEngGetPrinterDataFileName\nEngLoadModule\nEngMapModule\nEngFreeModule\nEngQueryLocalTime\nEngComputeGlyphSet\nEngCreateSemaphore\nEngAcquireSemaphore\nEngReleaseSemaphore\nEngDeleteSemaphore\n' > "$T/eng.def"
"${MINGW%-gcc}-dlltool" -d "$T/eng.def" -l "$T/libeng.a" || { echo "SKIP: no dlltool"; exit 77; }
"$MINGW" -shared -O2 -o "$T/sgtestdrv.dll" "$HERE/drvpkg-umpd.c" "$T/libeng.a" -lgdi32 -lwinspool || { echo "FAIL  driver did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }

BOOT_START=$(date +%s)
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; BOOT_RC=$?; "$WINESERVER" -w
BOOT_TIME=$(( $(date +%s) - BOOT_START ))
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$T/probe32.exe" "$C/"

# the package, as a maker's installer leaves it in the driver store
S="$C/windows/system32/DriverStore/FileRepository/sgmaker.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgtestdrv.dll" "$S/amd64/"
printf 'SG data v1\r\nmore\r\n' > "$T/sgmaker.dat"
python3 "$HERE/drvpkg-pack.py" szdd "$T/sgmaker.dat" "$S/sgmaker.da_"
printf 'SG extra from the cabinet\r\n' > "$T/sgextra.txt"
python3 "$HERE/drvpkg-pack.py" cab "$S/sgfiles.cab" "$T/sgextra.txt"
# a file installed under another name from a cabinet of that one file (OKI's)
mkdir -p "$T/one"
printf 'SG from a one-file cabinet\r\n' > "$T/one/inner.txt"
python3 "$HERE/drvpkg-pack.py" cab "$S/sgsingle.tx_" "$T/one/inner.txt"
printf 'SG system file\r\n' > "$S/sgsys.dat"
# a maker's own copies of the core driver: never ours to load
printf 'not a DLL\r\n' > "$S/UNIDRV.DLL"
printf 'not a DLL\r\n' > "$S/UNIDRVUI.DLL"
cat > "$S/sgcore.gpd" <<'EOF'
*GPDSpecVersion: "1.0"
*GPDFileName: "sgcore.gpd"
*ModelName: "SG Test Core"
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
        *Name: "SG Core 2 x 1"
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
cat > "$S/sgmaker.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Old" = SGOLD
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Test Maker" = SGM_INSTALL
"SG Test Core" = SGC_INSTALL

[SGOLD]
"SG Old Model" = SGM_INSTALL
"SG Test Core" = SGM_INSTALL
"SG Data Only" = SGD_INSTALL

[SGD_INSTALL]
CopyFiles=@sgcore.gpd
DataFile=sgcore.gpd
DriverFile=UNIDRV.DLL
ConfigFile=UNIDRVUI.DLL

[SGM_INSTALL]
CopyFiles=SGM_FILES, SGM_SYS
DataSection=SGM_DATA
AddReg=SGM_REG

[SGM_REG]
HKLM,"Software\SG Test Maker","Installed",,"yes"

[SGM_DATA]
DriverFile=sgmaker.dll
ConfigFile=sgmaker.dll
DataFile=sgmaker.dat

[SGM_FILES]
sgmaker.dll,sgtestdrv.dll
sgmaker.dat
sgextra.txt
sgrenamed.txt,sgsingle.tx_

[SGM_SYS]
sgsys.dat

[SGC_INSTALL]
CopyFiles=@sgcore.gpd, @UNIDRV.DLL, @UNIDRVUI.DLL
DataFile=sgcore.gpd
DriverFile=UNIDRV.DLL
ConfigFile=UNIDRVUI.DLL

[DestinationDirs]
DefaultDestDir=66000
SGM_SYS=66002

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,
3 = "SG test cabinet",sgfiles.cab,,

[SourceDisksFiles]
sgtestdrv.dll = 1
sgmaker.dat = 2
sgextra.txt = 3
sgsingle.tx_ = 2
sgsys.dat = 2
sgcore.gpd = 2
UNIDRV.DLL = 2
UNIDRVUI.DLL = 2
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\probe64.exe' install "SG Test Maker")
[ "$out" = "install 0" ] && pass "the maker's package installs" || fail "the maker's package did not install: $out"
out=$(run 'C:\probe64.exe' install "SG Old Model")
[ "$out" != "install 0" ] && pass "a model listed only for 32-bit Windows is not installed ($out)" ||
    fail "a 32-bit-only model was installed on 64-bit"
out=$(run 'C:\probe64.exe' install "SG Data Only")
[ "$out" = "install 0" ] && pass "a model listed only for 32-bit Windows installs when it is all data on the core drivers" ||
    fail "a data-only 32-bit-listed model did not install: $out"
out=$(run 'C:\probe64.exe' install "SG Test Core")
[ "$out" = "install 0" ] && pass "the core driver package installs" || fail "the core driver package did not install: $out"
"$WINESERVER" -w

D="$C/windows/system32/spool/drivers/x64/3"
head -c 2 "$D/sgmaker.dll" 2>/dev/null | grep -q MZ && pass "a file installed under another name (destination,source)" ||
    fail "sgmaker.dll (from sgtestdrv.dll) not in the driver directory"
[ "$(head -1 "$D/sgmaker.dat" 2>/dev/null | tr -d '\r')" = "SG data v1" ] && pass "a file compressed for setup is expanded" ||
    fail "sgmaker.dat not expanded: $(head -c 20 "$D/sgmaker.dat" 2>/dev/null | od -c | head -1)"
[ "$(tr -d '\r' < "$D/sgextra.txt" 2>/dev/null)" = "SG extra from the cabinet" ] && pass "a file from the source disk's cabinet" ||
    fail "sgextra.txt not taken from the cabinet"
timeout 60 "$WINE" reg query 'HKLM\Software\SG Test Maker' /v Installed 2>/dev/null | tr -d '\r' | grep -q "Installed.*REG_SZ.*yes" &&
    pass "the install section's AddReg entries are made" || fail "no AddReg entry"
[ "$(tr -d '\r' < "$D/sgrenamed.txt" 2>/dev/null)" = "SG from a one-file cabinet" ] &&
    pass "a file installed under another name from a compressed source is expanded" ||
    fail "sgrenamed.txt: $(head -c 8 "$D/sgrenamed.txt" 2>/dev/null | od -c | head -1)"
[ -f "$C/windows/system32/sgsys.dat" ] && pass "a file for the system directory (66002) lands there" ||
    fail "sgsys.dat not in system32"
head -c 2 "$D/UNIDRV.DLL" 2>/dev/null | grep -q MZ && pass "the core driver's files are ours, not the package's copies" ||
    fail "the package's own UNIDRV.DLL was installed"

for p in LPT1 LPT2 LPT3; do
    timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "$p:" /d "$T/$p.out" /f >/dev/null 2>&1
done
run 'C:\probe64.exe' add "Maker Printer" "SG Test Maker" "LPT1:" >/dev/null
run 'C:\probe64.exe' add "Core Printer" "SG Test Core" "LPT2:" >/dev/null
timeout 60 "$WINE" reg query 'HKLM\Software\SG Test Maker\Printers' /v "Maker Printer" 2>/dev/null | tr -d '\r' |
    grep -q "REG_SZ *initialized" && pass "a new printer's driver is told so (DrvPrinterEvent) and sets itself up" ||
    fail "the driver was not told of its new printer"

out=$(run 'C:\probe64.exe' papers "Maker Printer")
[ "$out" = "paper 257 533x279 SG Maker 2x1" ] && pass "the maker's configuration DLL reads the printer through its handle" ||
    fail "DeviceCapabilities through the maker's configuration DLL: $out"

timeout 60 "$WINE" reg delete 'HKCU\Software\SG Test Maker' /f >/dev/null 2>&1
rm -f "$T/LPT1.out"
run 'C:\probe64.exe' print "Maker Printer" >/dev/null
events=$(timeout 60 "$WINE" reg query 'HKCU\Software\SG Test Maker' /v Events 2>/dev/null | tr -d '\r' | awk '/Events/ {print $3}')
[ "$events" = "14,1,2,5,13,6,7,8,12,R,10" ] &&
    pass "the driver hears GDI's document events, the end before its job is printed (1035)" ||
    fail "document events: $events (want the filter query 14, CreateDC 1,2, StartDoc 5,13, page 6,7, EndDoc 8,12, rendering, DeleteDC 10)"

run 'C:\probe64.exe' add "Quiet Printer" "SG Test Maker" "LPT3:" >/dev/null
timeout 60 "$WINE" reg delete 'HKCU\Software\SG Test Maker' /f >/dev/null 2>&1
run 'C:\probe64.exe' print "Quiet Printer" >/dev/null
events=$(timeout 60 "$WINE" reg query 'HKCU\Software\SG Test Maker' /v Events 2>/dev/null | tr -d '\r' | awk '/Events/ {print $3}')
[ "$events" = "14,1,R" ] && pass "a driver that answers the DC's creation with failure hears nothing more, and prints" ||
    fail "after failure at CreateDC: $events"

page() { printf 'SGM START printdrv probe data=SG data v1 time=ok density=%s job=ok glyphs=ok\nPAGE 1 200x100 dark=5000 box=20,10-119,59\nSGM END' "$1"; }
for b in 64 32; do
    rm -f "$T/LPT1.out" "$T/LPT2.out"
    run "C:\\probe$b.exe" print "Maker Printer" >/dev/null
    out=$(tr -d '\r' < "$T/LPT1.out" 2>/dev/null)
    [ "$out" = "$(page 3)" ] &&
        pass "$b-bit: the page reached the driver's own surface, its WritePrinter reached the port, it found its job" ||
        { fail "$b-bit: the maker-style driver's output is not the page"; printf '      %s\n' "$out"; }
done

# a 32-bit program and the 64-bit driver: our print host answers for its
# configuration DLL (settings, papers) and gives it its job
out=$(run 'C:\probe32.exe' papers "Maker Printer")
[ "$out" = "paper 257 533x279 SG Maker 2x1" ] && pass "32-bit: the 64-bit configuration DLL's papers, through the print host" ||
    fail "32-bit DeviceCapabilities: $out"
rm -f "$T/LPT1.out"
out=$(run 'C:\probe32.exe' printd "Maker Printer" 7)
pages=$(tr -d '\r' < "$T/LPT1.out" 2>/dev/null)
[ "$out" = "devmode 220+8
printd ok" ] && [ "$pages" = "$(page 7)" ] &&
    pass "32-bit: the driver's own settings, from the print host, reach the driver" ||
    { fail "32-bit: the driver's own settings: $out"; printf '      %s\n' "$pages"; }
for b in 64 32; do
    rm -f "$T/LPT1.out"
    out=$(run "C:\\probe$b.exe" printd "Maker Printer")
    pages=$(tr -d '\r' < "$T/LPT1.out" 2>/dev/null)
    [ "$out" = "printd ok" ] && [ "$pages" = "$(page 3)" ] &&
        pass "$b-bit: a program's public settings alone are merged into the driver's own" ||
        { fail "$b-bit: public settings alone: $out"; printf '      %s\n' "$pages"; }
done

run 'C:\probe64.exe' print "Core Printer" >/dev/null
blocks=$(grep -a -o "$(printf '\033')\*b[0-9]*W" "$T/LPT2.out" 2>/dev/null | wc -l)
[ "$blocks" = 50 ] && pass "the core driver prints although the package carried its own copies" ||
    fail "Unidrv from the core package: $blocks rows"

diagnose
[ $RC = 0 ] && echo "drvpkg gate: PASS" || echo "drvpkg gate: FAIL"
exit $RC
