#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A printer maker's own print processor (patches/sg/1030, 1038, 1041), as Canon's and
# HP's packages carry: our own (test/printproc-fixture.c) installed by a
# package's PrintProcessor= line, on a PostScript printer:
#   - an EMF job is handed to it; through GDI's print processor functions
#     it finds the job's pages (GdiGetSpoolFileHandle, GdiGetPageCount,
#     GdiGetPageHandle), gets a DC of the printer (GdiGetDC) and plays each
#     page on it (GdiStartDocEMF, GdiPlayPageEMF...), adding a mark;
#   - what it draws on that DC is the job, sent to the job's port (not a
#     new job): one PostScript document of the two pages, with its mark.
#   WINE=/opt/wine-sg/bin/wine test/printproc-gate.sh
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
T=$(mktemp -d /var/tmp/sg-printproc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

"$MINGW" -municode -O2 -o "$T/sgp64.exe" "$HERE/../tools/printer-corpus/sgprint.c" -lwinspool -lgdi32 -lsetupapi ||
    { echo "FAIL  sgprint did not build"; exit 1; }
"$MINGW" -DUNICODE -shared -O2 -o "$T/sgproc.dll" "$HERE/printproc-fixture.c" -lgdi32 -lwinspool ||
    { echo "FAIL  processor did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/sgp64.exe" "$C/"

S="$C/windows/system32/DriverStore/FileRepository/sgproc.inf_1"
mkdir -p "$S"
cp "$T/sgproc.dll" "$S/"
cat > "$S/sgproc.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGPROC.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG Proc PS"
*NickName: "SG Proc PS"
*ShortNickName: "SG Proc PS"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 20 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 20 AnySetup *PageRegion
*DefaultPageRegion: Letter
*PageRegion Letter/Letter: "<</PageSize[612 792]>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/Letter: "18 18 594 774"
*DefaultPaperDimension: Letter
*PaperDimension Letter/Letter: "612 792"
*DefaultResolution: 300dpi
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
cat > "$S/sgproc.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Proc PS" = SGPROC_INSTALL

[SGPROC_INSTALL]
CopyFiles=@sgproc.ppd
DataFile=sgproc.ppd
PrintProcessor="SG Test Processor,sgproc.dll"
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames]
1 = "SG test disk",,,

[SourceDisksFiles]
sgproc.ppd = 1
sgproc.dll = 1
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\sgp64.exe' install "SG Proc PS")
[ "$out" = "install 0" ] && pass "the package with a print processor installs" || fail "install: $out"
"$WINESERVER" -w
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$T/LPT1.out" /f >/dev/null 2>&1
out=$(run 'C:\sgp64.exe' add "Proc Printer" "SG Proc PS" "LPT1:" "SG Test Processor")
[ "$out" = "add ok" ] && pass "a printer on the maker's processor" || fail "add: $out"
export SG_PRINTPROC_LOG='C:\printproc.log'
out=$(run 'C:\sgp64.exe' print "Proc Printer")
case "$out" in "print ok"*) pass "the job printed" ;; *) fail "print: $out" ;; esac
L=$(tr -d '\r' < "$C/printproc.log" 2>/dev/null | tr '\n' ';')
[ "$L" = "open NT EMF 1.008;pages 2 dc 1;page 1 played 1;page 2 played 1;done 1;" ] &&
    pass "the processor found the two pages, a DC, and played them" || fail "the processor's log: '$L'"
F="$T/LPT1.out"
n=$(grep -a -c '^%%EOF' "$F" 2>/dev/null)
[ "${n:-0}" = 1 ] && pass "its DC's job went to the job's port, once" || fail "%%EOF count: '$n'"
p=$(grep -a -c '^%%Page:' "$F" 2>/dev/null)
[ "${p:-0}" = 2 ] && pass "both pages are in the job" || fail "pages in the job: '$p'"
n=$(grep -a -c "findfont" "$F" 2>/dev/null)
[ "${n:-0}" -gt 10 ] && pass "the pages' own drawing (the test page's text) was played on the DC" ||
    fail "the pages were not played ($n fonts)"
grep -a -q "1 0 0 setrgbcolor\|1.0* 0.0* 0.0* setrgbcolor" "$F" 2>/dev/null && pass "the processor's own mark is on the pages" ||
    fail "no red mark"
if command -v gs >/dev/null; then
    errs=$(gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=nullpage "$F" 2>&1 >/dev/null | grep -c -i error)
    [ "$errs" = 0 ] && pass "ghostscript reads the job" || fail "ghostscript found errors"
fi
# the page lands where winprint puts it (1041): the same driver on winprint
# prints the reference; the processor plays into the sheet's rectangle and
# into none; the two pages' ink must agree (its red mark aside)
if command -v gs >/dev/null; then
    run 'C:\sgp64.exe' add "Plain Printer" "SG Proc PS" "LPT2:" winprint >/dev/null
    timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT2:" /d "$T/LPT2.out" /f >/dev/null 2>&1
    timeout 60 "$WINE" reg add 'HKLM\System\CurrentControlSet\Control\Print\Printers\Plain Printer' /v "Print Processor" /d winprint /f >/dev/null 2>&1
    run 'C:\sgp64.exe' print "Plain Printer" >/dev/null
    gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pgmraw -r20 -dFirstPage=1 -dLastPage=1 -sOutputFile="$T/ref.pgm" "$T/LPT2.out" 2>/dev/null
    for mode in rect norect emptyrect; do
        rm -f "$T/LPT1.out"
        case $mode in
        norect) SG_PRINTPROC_NORECT=1 run 'C:\sgp64.exe' print "Proc Printer" >/dev/null ;;
        emptyrect) SG_PRINTPROC_EMPTYRECT=1 run 'C:\sgp64.exe' print "Proc Printer" >/dev/null ;;
        *) run 'C:\sgp64.exe' print "Proc Printer" >/dev/null ;;
        esac
        gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pgmraw -r20 -dFirstPage=1 -dLastPage=1 -sOutputFile="$T/$mode.pgm" "$T/LPT1.out" 2>/dev/null
        m=$(python3 - "$T/ref.pgm" "$T/$mode.pgm" <<'PY'
import sys
def load(p):
    d = open(p, 'rb').read()
    head, pos = [], 0
    while len(head) < 4:
        while d[pos:pos + 1].isspace(): pos += 1
        if d[pos:pos + 1] == b'#':
            pos = d.index(b'\n', pos) + 1
            continue
        end = pos
        while not d[end:end + 1].isspace(): end += 1
        head.append(d[pos:end]); pos = end
    w, h = int(head[1]), int(head[2])
    return w, h, d[-w * h:]
w1, h1, a = load(sys.argv[1]); w2, h2, b = load(sys.argv[2])
if (w1, h1) != (w2, h2): print(0); sys.exit()
ink = [i for i in range(w1 * h1) if a[i] < 160 or b[i] < 160]
same = sum(1 for i in ink if (a[i] < 160) == (b[i] < 160))
print(round(same / max(1, len(ink)), 2))
PY
)
        ok=$(python3 -c "print(1 if float('${m:-0}') >= 0.9 else 0)")
        [ "$ok" = 1 ] && pass "the processor's page ($mode) lands where winprint puts it ($m)" ||
            fail "the processor's page ($mode) is not where winprint puts it ($m)"
    done
fi

# a maker's processor that gives up without reading the job (1038): the job
# is printed by winprint, through the driver
rm -f "$T/LPT1.out" "$C/printproc.log"
out=$(SG_PRINTPROC_QUIT=1 run 'C:\sgp64.exe' print "Proc Printer")
L=$(tr -d '\r' < "$C/printproc.log" 2>/dev/null | tr '\n' ';')
p=$(grep -a -c '^%%Page:' "$T/LPT1.out" 2>/dev/null)
[ "$L" = "open NT EMF 1.008;quit;" ] && [ "${p:-0}" = 2 ] &&
    pass "a processor that gives up without reading the job: winprint prints it" ||
    fail "after the processor gave up: log '$L', pages '$p'"

if [ $RC != 0 ]; then echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme" | tail -15; fi
[ $RC = 0 ] && echo "printproc gate: PASS" || echo "printproc gate: FAIL"
exit $RC
