#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32: SHInvokePrinterCommandW/A as Microsoft documents them (patches/sg/9923; test/printcmd-probe.c). They
# were spec stubs, so a caller crashed. A PostScript driver package is installed and "SG Cmd Printer" made on LPT1:
# (a file); the probe checks the errors for unknown printers/actions and server properties, prints test pages
# (modal and not: each job, titled "Test Page", reaches the port), makes a shortcut to the printer, and sees the
# properties and printing preferences dialogs open.
# Mutant: SG_MUTANT_PRINTCMD_NO_TESTPAGE (dlls/shell32/shellord.c). Windows: SG_GATE_DISPLAY=:N, else an Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/printcmd-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-printcmd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export CUPS_SERVER="$T/no-cups.sock"
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
{ TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$T/sgp64.exe" "$HERE/../tools/printer-corpus/sgprint.c" -lwinspool -lgdi32 -lsetupapi &&
  TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/printcmd-probe.exe" "$HERE/printcmd-probe.c" -lshell32 -lole32 -luuid
} 2>"$T/cc.log" || { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
mkdir -p "$WINEPREFIX"
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/sgp64.exe" "$T/printcmd-probe.exe" "$C/"
S="$C/windows/system32/DriverStore/FileRepository/sgcmd.inf_1"
mkdir -p "$S"
cat > "$S/sgcmd.ppd" <<'PPD'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGCMD.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG Cmd PS"
*NickName: "SG Cmd PS"
*ShortNickName: "SG Cmd PS"
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
PPD
cat > "$S/sgcmd.inf" <<'INF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Cmd PS" = SGCMD_INSTALL

[SGCMD_INSTALL]
CopyFiles=@sgcmd.ppd
DataFile=sgcmd.ppd
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[DestinationDirs]
DefaultDestDir=66000

[SourceDisksNames]
1 = "SG test disk",,,

[SourceDisksFiles]
sgcmd.ppd = 1
INF
run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\sgp64.exe' install "SG Cmd PS")
[ "$out" = "install 0" ] && pass "the PostScript package installs" || fail "install: $out"
"$WINESERVER" -w
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$T/LPT1.out" /f >/dev/null 2>&1
out=$(run 'C:\sgp64.exe' add "SG Cmd Printer" "SG Cmd PS" "LPT1:")
[ "$out" = "add ok" ] && pass "the printer SG Cmd Printer on LPT1:" || fail "add: $out"
# each job rewrites the port's file: the modal page first, then the rest (a page printed on its own thread)
check_job() {
    if grep -a -q '^%%EOF' "$T/LPT1.out" 2>/dev/null && grep -a -q '^%%Title: Test Page' "$T/LPT1.out"; then
        pass "$1: a job titled Test Page reached the port"
    else fail "$1: no Test Page job at the port"; fi
}
run 'C:\printcmd-probe.exe' page > "$T/probe1.out"
cat "$T/probe1.out"
grep -qx 'RESULT: PASS' "$T/probe1.out" || RC=1
check_job "modal"
rm -f "$T/LPT1.out"
run 'C:\printcmd-probe.exe' > "$T/probe.out"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" || RC=1
check_job "not modal"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
