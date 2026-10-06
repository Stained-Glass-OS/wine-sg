#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A PostScript driver's render plug-in (patches/sg/1024), as printer makers
# ship them for job accounting, user codes and finishing: our own plug-in
# (test/psplugin-plugin.c, IPrintOemPS2) on our own PPD, named by the
# package's .ini ([OEMFiles] OEMDriverFile1).
#   - the core loads it and calls it at the job's injection points: the
#     stream's start (before the JCL), the prolog, the setup, each page's
#     setup, the end of file, the stream's end;
#   - what it writes at the stream's start goes through the DEVOBJ's
#     DRVPROCS table (older plug-ins write that way), the rest through the
#     core's helper (IPrintOemDriverPS::DrvWriteSpoolBuf);
#   - the helper answers its question for the chosen ColorModel option
#     (the program's dmColor chose Gray), and IPrintCorePS2 lists the
#     PPD's features (those wineps handles itself too) and the chosen
#     PageSize;
#   - its WritePrinter sees the job on its way out: the byte counts it
#     reports grow, and nothing is lost on the way to the port;
#   - ghostscript reads the job.
#   WINE=/opt/wine-sg/bin/wine test/psplugin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-psplugin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
diagnose() { [ $RC = 0 ] && return; echo "--- wineboot: rc $BOOT_RC, ${BOOT_TIME}s; drive_c: $(ls "$C" 2>&1 | tr '\n' ' ')"; tail -3 "$T/wineboot.log"; echo "--- printers:"; run 'C:\sgp64.exe' enum 2>/dev/null; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme\|wayland" | tail -15; }

"$MINGW" -municode -O2 -o "$T/sgp64.exe" "$HERE/../tools/printer-corpus/sgprint.c" -lwinspool -lgdi32 -lsetupapi ||
    { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -shared -O2 -o "$T/sgpsplug.dll" "$HERE/psplugin-plugin.c" -lole32 -luuid -lwinspool ||
    { echo "FAIL  plug-in did not build"; exit 1; }
BOOT_START=$(date +%s)
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; BOOT_RC=$?; "$WINESERVER" -w
BOOT_TIME=$(( $(date +%s) - BOOT_START ))
C="$WINEPREFIX/drive_c"
cp "$T/sgp64.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sgpsp.inf_1"
mkdir -p "$S"
cp "$T/sgpsplug.dll" "$S/"
printf '[OEMFiles]\r\nOEMDriverFile1=sgpsplug.dll\r\n' > "$S/sgpsp.ini"
cat > "$S/sgpsp.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGPSP.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG Plug-in PS"
*NickName: "SG Plug-in PS"
*ShortNickName: "SG Plug-in PS"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*TTRasterizer: Type42
*Protocols: PJL
*JCLBegin: "<1B>%-12345X@PJL JOB<0A>"
*JCLToPSInterpreter: "@PJL ENTER LANGUAGE=POSTSCRIPT<0A>"
*JCLEnd: "<1B>%-12345X@PJL EOJ<0A><1B>%-12345X"
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 20 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 20 AnySetup *PageRegion
*DefaultPageRegion: Letter
*PageRegion Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/Letter: "18 18 594 774"
*DefaultPaperDimension: Letter
*PaperDimension Letter/Letter: "612 792"
*OpenUI *ColorModel/Color: PickOne
*OrderDependency: 30 AnySetup *ColorModel
*DefaultColorModel: RGB
*ColorModel RGB/Color: "<</ProcessColorModel /DeviceRGB>>setpagedevice"
*ColorModel Gray/Grayscale: "<</ProcessColorModel /DeviceGray>>setpagedevice"
*CloseUI: *ColorModel
*OpenUI *SGStaple/Staple: PickOne
*OrderDependency: 40 DocumentSetup *SGStaple
*DefaultSGStaple: None
*SGStaple None/None: ""
*SGStaple One/One Staple: "% staple"
*CloseUI: *SGStaple
*DefaultResolution: 300dpi
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
cat > "$S/sgpsp.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Plug-in PS" = SGPSP_INSTALL

[SGPSP_INSTALL]
CopyFiles=@sgpsp.ppd, @sgpsp.ini, @sgpsplug.dll
DataFile=sgpsp.ppd
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames]
1 = "SG test disk",,,

[SourceDisksFiles]
sgpsp.ppd = 1
sgpsp.ini = 1
sgpsplug.dll = 1
EOF

out=$(run 'C:\sgp64.exe' install "SG Plug-in PS")
[ "$out" = "install 0" ] && pass "the package with a plug-in installs" || fail "the package did not install: $out"
"$WINESERVER" -w
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$T/LPT1.out" /f >/dev/null 2>&1
run 'C:\sgp64.exe' add "Plug-in Printer" "SG Plug-in PS" "LPT1:" >/dev/null

out=$(run 'C:\sgp64.exe' print "Plug-in Printer" color=1)
case "$out" in "print ok"*) pass "the job printed" ;; *) fail "print: $out" ;; esac
F="$T/LPT1.out"
[ -s "$F" ] || F=/dev/null
marks=$(grep -a -o '%SG[0-9]* [A-Za-z?]* [0-9]*' "$F" | cut -d' ' -f1 | tr '\n' ' ')
[ "$marks" = "%SG1 %SG14 %SG16 %SG101 %SG101 %SG19 %SG20 " ] &&
    pass "the plug-in added its code at the stream's start, the prolog, the setup, both pages, the end" ||
    fail "the plug-in's injections: '$marks'"
head -c 40 "$F" | grep -a -q '^%SG1 ' && pass "the stream's start comes before the JCL (written through DRVPROCS)" ||
    fail "the first bytes: $(head -c 30 "$F" | tr -d '\033' | tr '\n' '|')"
awk '/^%SG14 /{p=1} p&&/^%!PS-Adobe/{print "bad"} /^%%EndProlog/{p=0}' "$F" | grep -q bad &&
    fail "the prolog injection is before the header" || true
grep -a -B1 '^%SG16 ' "$F" | head -1 | grep -q '^%%BeginSetup' && pass "the setup injection follows %%BeginSetup" ||
    fail "the setup injection is not right after %%BeginSetup"
color=$(grep -a '^%SG16 ' "$F" | cut -d' ' -f2)
[ "$color" = Gray ] && pass "DrvGetDriverSetting gave the chosen ColorModel (Gray)" || fail "ColorModel: '$color'"
nf=$(grep -a '^%SG16 ' "$F" | cut -d' ' -f3)
[ "${nf:-0}" -ge 4 ] && pass "IPrintCorePS2 listed the PPD's features ($nf)" || fail "IPrintCorePS2 features: '$nf'"
page=$(grep -a '^%SG16 ' "$F" | cut -d' ' -f5)
[ "$page" = Letter ] && pass "IPrintCorePS2::GetOptions gave the chosen PageSize (Letter)" || fail "PageSize: '$page'"
seen=$(grep -a -o '%SG[0-9]* [A-Za-z?]* [0-9]* [0-9]*' "$F" | cut -d' ' -f4 | tr '\n' ' ')
last=$(echo "$seen" | awk '{print $NF}')
first=$(echo "$seen" | awk '{print $1}')
[ "${first:-x}" = 0 ] && [ "${last:-0}" -gt 1000 ] && pass "the plug-in's WritePrinter saw the job go by ($seen)" ||
    fail "the plug-in's WritePrinter saw: '$seen'"
tail -c 60 "$F" | grep -a -q "@PJL EOJ" && pass "the job reached the port whole (ends with *JCLEnd)" ||
    fail "the end of the job: $(tail -c 40 "$F" | tr -d '\033' | tr '\n' '|')"
if command -v gs >/dev/null; then
    errs=$(gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=nullpage "$F" 2>&1 >/dev/null | grep -c -i error)
    [ "$errs" = 0 ] && pass "ghostscript reads the job without error" || fail "ghostscript found errors"
fi

diagnose
[ $RC = 0 ] && echo "psplugin gate: PASS" || echo "psplugin gate: FAIL"
exit $RC
