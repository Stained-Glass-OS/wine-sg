#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The printer drivers' option sheets and the makers' UI plug-ins
# (patches/sg/1027): a GPD printer on our Unidrv and a PPD printer on our
# PostScript driver, each with our own UI plug-in (test/drvui-plugin.c,
# IPrintOemUI2) named by the package's .ini (OEMConfigFile1); the program
# (test/drvui-probe.c) opens their preferences and properties and works
# them like a user:
#   - the document sheet has Layout, Paper/Quality and Advanced, and the
#     plug-in's own page;
#   - the plug-in sees the driver's options when it adds its own
#     (CommonUIProp), and the core's features (IPrintCoreUI2);
#   - choosing a paper size and the plug-in's option, then OK: IDOK, the
#     devmode has the paper, the plug-in got APPLYNOW with the paper chosen,
#     and (Unidrv) its part of the devmode keeps its option;
#   - the printer's sheet (PrinterProperties) shows the printer's own
#     features (*FeatureType: PRINTER_PROPERTY, PPD InstallableOptions)
#     and the plug-in's page; a choice there is kept with the printer.
#   WINE=/opt/wine-sg/bin/wine test/drvui-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-drvui.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
for i in $(seq 50); do [ -s "$T/display" ] && break; sleep 0.1; done
export DISPLAY=":$(cat "$T/display")"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
"$MINGW" -DUNICODE -shared -O2 -o "$T/sgdrvui.dll" "$HERE/drvui-plugin.c" -lole32 -luuid || { echo "FAIL  plug-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/drvui.exe" "$HERE/drvui-probe.c" -lwinspool -lcomctl32 || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/sgp64.exe" "$HERE/../tools/printer-corpus/sgprint.c" -lwinspool -lgdi32 -lsetupapi ||
    { echo "FAIL  sgprint did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/drvui.exe" "$T/sgp64.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sgdrvui.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgdrvui.dll" "$S/amd64/"
printf '[OEMFiles]\r\nOEMConfigFile1=sgdrvui.dll\r\n' > "$S/sgdrvui.ini"
cat > "$S/sgdrvui.gpd" <<'EOF'
*GPDSpecVersion: "1.0"
*GPDFileName: "sgdrvui.gpd"
*ModelName: "SG UI Laser"
*MasterUnits: PAIR(100, 100)
*Feature: Resolution
{
    *DefaultOption: R100
    *Option: R100 { *Name: "100 dpi" *DPI: PAIR(100, 100) }
}
*Feature: PaperSize
{
    *DefaultOption: SGSMALL
    *Option: SGSMALL
    {
        *Name: "SG Small"
        *PageDimensions: PAIR(200, 100)
        *PrintableArea: PAIR(200, 100)
        *PrintableOrigin: PAIR(0, 0)
    }
    *Option: SGBIG
    {
        *Name: "SG Big"
        *PageDimensions: PAIR(400, 200)
        *PrintableArea: PAIR(400, 200)
        *PrintableOrigin: PAIR(0, 0)
    }
}
*Feature: ColorMode
{
    *DefaultOption: Mono
    *Option: Mono { *Name: "Mono" *DevBPP: 1 *DevNumOfPlanes: 1 *DrvBPP: 1 }
}
*Feature: SGTray
{
    *Name: "SG Tray"
    *FeatureType: PRINTER_PROPERTY
    *DefaultOption: NotInstalled
    *Option: NotInstalled { *Name: "Not Installed" }
    *Option: Installed { *Name: "Installed" }
}
*Command: CmdSendBlockData { *Cmd : "<1B>*b" %d{NumOfDataBytes}"W" }
*Command: CmdFF { *Cmd : "<0C>" }
EOF
cat > "$S/sgdrvui.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGDRVUI.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG UI PS"
*NickName: "SG UI PS"
*ShortNickName: "SG UI PS"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: False
*DefaultColorSpace: Gray
*OpenGroup: InstallableOptions/Options Installed
*OpenUI *SGTray/SG Tray: PickOne
*DefaultSGTray: False
*SGTray False/Not Installed: ""
*SGTray True/Installed: ""
*CloseUI: *SGTray
*CloseGroup: InstallableOptions
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 20 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]>>setpagedevice"
*PageSize A4/A4: "<</PageSize[595 842]>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 20 AnySetup *PageRegion
*DefaultPageRegion: Letter
*PageRegion Letter/Letter: "<</PageSize[612 792]>>setpagedevice"
*PageRegion A4/A4: "<</PageSize[595 842]>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/Letter: "18 18 594 774"
*ImageableArea A4/A4: "18 18 577 824"
*DefaultPaperDimension: Letter
*PaperDimension Letter/Letter: "612 792"
*PaperDimension A4/A4: "595 842"
*DefaultResolution: 300dpi
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
cat > "$S/sgdrvui.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG UI Laser" = SGUNI_INSTALL
"SG UI PS" = SGPS_INSTALL

[SGUNI_INSTALL]
CopyFiles=@sgdrvui.gpd, @sgdrvui.dll, @sgdrvui.ini
DataFile=sgdrvui.gpd
CoreDriverSections="{D20EA372-DD35-4950-9ED8-A6335AFE79F0},UNIDRV.OEM,UNIDRV_DATA"

[SGPS_INSTALL]
CopyFiles=@sgdrvui.ppd, @sgdrvui.dll, @sgdrvui.ini
DataFile=sgdrvui.ppd
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames.amd64]
1 = "SG test disk",,,"amd64"
2 = "SG test disk",,,

[SourceDisksFiles]
sgdrvui.dll = 1
sgdrvui.ini = 2
sgdrvui.gpd = 2
sgdrvui.ppd = 2
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
for m in "SG UI Laser" "SG UI PS"; do
    out=$(run 'C:\sgp64.exe' install "$m")
    [ "$out" = "install 0" ] && pass "$m installs" || fail "$m: $out"
done
"$WINESERVER" -w
run 'C:\sgp64.exe' add "UI Laser" "SG UI Laser" "LPT1:" >/dev/null
run 'C:\sgp64.exe' add "UI PS" "SG UI PS" "LPT2:" >/dev/null
export SG_DRVUI_LOG='C:\drvui.log'
LOG="$C/drvui.log"
has() { printf '%s\n' "$1" | grep -qxF "$2"; }
check() { if has "$2" "$3"; then pass "$1"; else fail "$1 (wanted '$3')"; fi; }

for kind in "UI Laser:SG Small:SG Big" "UI PS:Letter:A4"; do
    p=${kind%%:*}; rest=${kind#*:}; first=${rest%%:*}; second=${rest#*:}
    rm -f "$LOG"
    OUT=$(run 'C:\drvui.exe' doc "$p")
    L=$(tr -d '\r' < "$LOG" 2>/dev/null)
    check "$p: the document sheet's pages, the plug-in's last" "$OUT" "tabs [Layout] [Paper/Quality] [Advanced] [SG Page]"
    check "$p: the plug-in sees the driver's options when it adds its own" "$L" "commonuiprop 1 first drv items 1"
    if has "$L" "features 0" || ! printf '%s\n' "$L" | grep -q '^features [1-9]'; then fail "$p: IPrintCoreUI2 lists no features"; else pass "$p: IPrintCoreUI2 lists the features"; fi
    check "$p: the plug-in's option is in the Advanced tree" "$OUT" "tree item SG Stamp: Off"
    check "$p: the plug-in hears its option change" "$L" "stamp changed 2"
    check "$p: OK gives IDOK" "$OUT" "documentproperties IDOK"
    check "$p: the paper chosen is in the devmode" "$OUT" "after form $second"
    if printf '%s\n' "$L" | grep -q "^apply stamp 2"; then pass "$p: the plug-in got APPLYNOW"; else fail "$p: no APPLYNOW for the plug-in"; fi
    [ "$p" = "UI Laser" ] && check "$p: the plug-in's part of the devmode keeps its option" "$OUT" "stamp 2"
    OUT=$(run 'C:\drvui.exe' cancel "$p")
    check "$p: Cancel gives IDCANCEL" "$OUT" "documentproperties IDCANCEL"
    rm -f "$LOG"
    OUT=$(run 'C:\drvui.exe' dev "$p")
    check "$p: the printer's sheet: Device Settings and the plug-in's page" "$OUT" "tabs [Device Settings] [SG Device]"
    check "$p: the printer's own feature is in Device Settings" "$OUT" "tree item after SG Tray: Installed"
    check "$p: PrinterProperties succeeds" "$OUT" "printerproperties 1"
    [ "$p" = "UI Laser" ] && check "$p: the printer's feature is kept with the printer" "$OUT" "printer feature Installed"
done

if [ $RC != 0 ]; then echo "--- last output:"; printf '%s\n' "$OUT" | tail -20; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme" | tail -15; fi
[ $RC = 0 ] && echo "drvui gate: PASS" || echo "drvui gate: FAIL"
exit $RC
