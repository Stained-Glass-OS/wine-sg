#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A printer maker's PostScript driver (patches/sg/1021): a PPD on the
# system's PostScript driver (the INF needs PSCRIPT.OEM from NTPRINT.INF).
# Our own package and PPD; the package also carries a file named
# PSCRIPT5.DLL, as some makers ship the system's, which must not be used.
#   - it installs, and its printer is our PostScript driver with the PPD;
#   - DeviceCapabilities answers from the PPD (papers, collation, media types);
#   - a job starts with the PPD's JCL (*JCLBegin, the *JCLOpenUI options,
#     *JCLToPSInterpreter) and ends with *JCLEnd;
#   - every *OpenUI option's code is in the section its *OrderDependency
#     names, in its order (Prolog, DocumentSetup/AnySetup, PageSetup);
#   - the program's dmColor, dmCollate and dmMediaType choose the PPD's
#     options, copies are asked of the printer, and the PostScript is
#     valid (ghostscript reads it, when it is installed).
#   WINE=/opt/wine-sg/bin/wine test/psdriver-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-psdriver.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
# on a failure, what Wine said (a gate that fails only now and then)
diagnose() { [ $RC = 0 ] && return; echo "--- wineboot: rc $BOOT_RC, ${BOOT_TIME}s; drive_c: $(ls "$C" 2>&1 | tr '\n' ' ')"; tail -3 "$T/wineboot.log"; echo "--- printers:"; run 'C:\sgp64.exe' enum 2>/dev/null; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme\|wayland" | tail -15; }

SGP="$HERE/../tools/printer-corpus/sgprint.c"
"$MINGW" -municode -O2 -o "$T/sgp64.exe" "$SGP" -lwinspool -lgdi32 -lsetupapi || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW32" -municode -O2 -o "$T/sgp32.exe" "$SGP" -lwinspool -lgdi32 -lsetupapi || { echo "FAIL  probe did not build"; exit 1; }
BOOT_START=$(date +%s)
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; BOOT_RC=$?; "$WINESERVER" -w
BOOT_TIME=$(( $(date +%s) - BOOT_START ))
C="$WINEPREFIX/drive_c"
cp "$T/sgp64.exe" "$T/sgp32.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sgps.inf_1"
mkdir -p "$S"
printf 'not the system driver\r\n' > "$S/PSCRIPT5.DLL"
cat > "$S/sgps.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGPS.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG Test PS"
*NickName: "SG Test PS"
*ShortNickName: "SG Test PS"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*TTRasterizer: Type42
*LandscapeOrientation: Plus90
*Protocols: PJL
*JCLBegin: "<1B>%-12345X@PJL JOB NAME=<22>SG<22><0A>"
*JCLToPSInterpreter: "@PJL ENTER LANGUAGE=POSTSCRIPT<0A>"
*JCLEnd: "<1B>%-12345X@PJL EOJ<0A><1B>%-12345X"
*JCLOpenUI *JCLEconomode/Toner Saving: PickOne
*DefaultJCLEconomode: Off
*OrderDependency: 10 JCLSetup *JCLEconomode
*JCLEconomode On/On: "@PJL SET ECONOMODE=ON<0A>"
*JCLEconomode Off/Off: "@PJL SET ECONOMODE=OFF<0A>"
*JCLCloseUI: *JCLEconomode
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 20 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*PageSize A4/A4: "<</PageSize[595 842]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 20 AnySetup *PageRegion
*DefaultPageRegion: Letter
*PageRegion Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*PageRegion A4/A4: "<</PageSize[595 842]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/Letter: "18 18 594 774"
*ImageableArea A4/A4: "18 18 577 824"
*DefaultPaperDimension: Letter
*PaperDimension Letter/Letter: "612 792"
*PaperDimension A4/A4: "595 842"
*OpenGroup: Quality/Quality
*OpenUI *ColorModel/Color: PickOne
*OrderDependency: 30 AnySetup *ColorModel
*DefaultColorModel: RGB
*ColorModel RGB/Color: "<</ProcessColorModel /DeviceRGB>>setpagedevice"
*ColorModel Gray/Grayscale: "<</ProcessColorModel /DeviceGray>>setpagedevice"
*CloseUI: *ColorModel
*OpenUI *MediaType/Paper Type: PickOne
*OrderDependency: 40 DocumentSetup *MediaType
*DefaultMediaType: Plain
*MediaType Plain/Plain Paper: "<</MediaType (Plain)>>setpagedevice"
*MediaType Glossy/Glossy Paper: "<</MediaType (Glossy)>>setpagedevice"
*CloseUI: *MediaType
*CloseGroup: Quality
*OpenUI *Collate/Collate: Boolean
*OrderDependency: 35 AnySetup *Collate
*DefaultCollate: False
*Collate True/On: "<</Collate true>>setpagedevice"
*Collate False/Off: "<</Collate false>>setpagedevice"
*CloseUI: *Collate
*OpenUI *SGWatermark/Watermark: PickOne
*OrderDependency: 50 PageSetup *SGWatermark
*DefaultSGWatermark: Mark
*SGWatermark None/None: ""
*SGWatermark Mark/SG Mark: "% SG page mark"
*CloseUI: *SGWatermark
*OpenUI *SGProlog/Prolog Item: PickOne
*OrderDependency: 5 Prolog *SGProlog
*DefaultSGProlog: Yes
*SGProlog Yes/Yes: "/sgprolog true def"
*CloseUI: *SGProlog
*DefaultResolution: 300dpi
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
cat > "$S/sgps.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Test PS" = SGPS_INSTALL

[SGPS_INSTALL]
CopyFiles=@sgps.ppd, @PSCRIPT5.DLL
DataFile=sgps.ppd
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames]
1 = "SG test disk",,,

[SourceDisksFiles]
sgps.ppd = 1
PSCRIPT5.DLL = 1
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\sgp64.exe' install "SG Test PS")
[ "$out" = "install 0" ] && pass "the PostScript package installs" || fail "the PostScript package did not install: $out"
"$WINESERVER" -w
D="$C/windows/system32/spool/drivers/x64/3"
head -c 2 "$D/PSCRIPT5.DLL" 2>/dev/null | grep -q MZ && pass "PSCRIPT5.DLL is ours, not the package's copy" ||
    fail "the package's own PSCRIPT5.DLL was installed"
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$T/LPT1.out" /f >/dev/null 2>&1
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT2:" /d "$T/LPT2.out" /f >/dev/null 2>&1
run 'C:\sgp64.exe' add "PS Printer" "SG Test PS" "LPT1:" >/dev/null
run 'C:\sgp64.exe' add "PS Printer 32" "SG Test PS" "LPT2:" >/dev/null

out=$(run 'C:\sgp64.exe' papers "PS Printer" | tr '\n' ';')
[ "$out" = "paper 1 2159x2794 Letter;paper 9 2099x2970 A4;" ] && pass "DeviceCapabilities lists the PPD's papers" ||
    fail "papers: $out"
out=$(run 'C:\sgp64.exe' features "PS Printer" | grep -E '^(collate|color|mediatypes)' | tr '\n' ';')
[ "$out" = "color 1;collate 1;mediatypes 2;" ] && pass "DeviceCapabilities answers from the PPD's options ($out)" ||
    fail "features: $out"

# the program asks for grey, collation and two copies
out=$(run 'C:\sgp64.exe' print "PS Printer" color=1 collate=1 copies=2)
case "$out" in "print ok"*) pass "64-bit: the job printed" ;; *) fail "64-bit print: $out" ;; esac
F="$T/LPT1.out"
jcl=$(tr -d '\r' < "$F" | sed -n '1,4p' | tr '\n' '|')
[ "$jcl" = "$(printf '\033')%-12345X@PJL JOB NAME=\"SG\"|@PJL SET ECONOMODE=OFF|@PJL ENTER LANGUAGE=POSTSCRIPT|%!PS-Adobe-3.0|" ] &&
    pass "the JCL options and the switch to PostScript come before the PostScript" || fail "JCL: $jcl"
tail -c 40 "$F" | grep -q "@PJL EOJ" && pass "the job ends with *JCLEnd" || fail "no *JCLEnd at the end"
order=$(grep -a -o '%%BeginFeature: \*[A-Za-z]* [A-Za-z0-9]*\|%%EndProlog\|%%BeginSetup\|%%EndSetup' "$F" | head -8 | tr '\n' ';')
[ "$order" = "%%BeginFeature: *SGProlog Yes;%%EndProlog;%%BeginSetup;%%BeginFeature: *PageSize Letter;%%BeginFeature: *ColorModel Gray;%%BeginFeature: *Collate True;%%BeginFeature: *MediaType Plain;%%EndSetup;" ] &&
    pass "the options' code is where and in the order the PPD says; dmColor and dmCollate chose Gray and Collate" ||
    fail "the options in the job: $order"
grep -a -A1 '%%BeginFeature: \*SGWatermark Mark' "$F" | grep -q '% SG page mark' && pass "a PageSetup option is on each page" ||
    fail "no PageSetup option"
grep -a -q '/NumCopies 2' "$F" && pass "copies are asked of the printer" || fail "no NumCopies"
if command -v gs >/dev/null; then
    pages=$(gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=nullpage "$F" 2>&1 >/dev/null | grep -c -i error)
    [ "$pages" = 0 ] && pass "ghostscript reads the job without error" || fail "ghostscript found errors"
fi

out=$(run 'C:\sgp32.exe' print "PS Printer 32" mediatype=257)
grep -a -q '%%BeginFeature: \*ColorModel' "$T/LPT2.out" 2>/dev/null && pass "32-bit: the job printed through the PPD" ||
    fail "32-bit: $out"

diagnose
[ $RC = 0 ] && echo "psdriver gate: PASS" || echo "psdriver gate: FAIL"
exit $RC
