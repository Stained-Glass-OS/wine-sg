#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A CUPS printer prints through its maker's Windows driver when one is
# installed (patches/sg/0924, 0925).
#
# CUPS's queue for a model ("SG Test UMPD") becomes a Windows printer with a
# PostScript driver made from the queue's PPD. When the maker's package for
# that model is in the driver store (an installer left it there), the
# printer is given the Windows driver instead, with the winprint print
# processor; the driver's output reaches the queue as a raw job, untouched by
# CUPS's filters. With the printer's "SgPrinterDriver" set to "linux" the
# PostScript driver is used, under "<model> (CUPS)". A queue we publish for
# Linux programs, "... (maker's driver)", is not a Windows printer. The gate
# runs a CUPS server of its own (no root, no printer).
#
#   WINE=/opt/wine-sg/bin/wine test/cupsdrv-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-cupsdrv.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
cat > "$T/root/cupsd.conf" <<EOF
Listen $T/cups.sock
LogLevel info
IdleExitTimeout 0
PreserveJobFiles Yes
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
ppd() {
    cat <<EOF
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGTEST.PPD"
*Manufacturer: "SG"
*Product: "($1)"
*ModelName: "$1"
*ShortNickName: "$1"
*NickName: "$1"
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
*DefaultPageSize: w144h72
*PageSize w144h72/Label: "<</PageSize[144 72]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
*DefaultPageRegion: w144h72
*PageRegion w144h72/Label: "<</PageSize[144 72]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: w144h72
*ImageableArea w144h72/Label: "0 0 144 72"
*DefaultPaperDimension: w144h72
*PaperDimension w144h72/Label: "144 72"
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
}
cat > "$T/root/printers.conf" <<EOF
<Printer SGLabel>
Info Label printer
MakeModel SG Test UMPD
DeviceURI file:///dev/null
State Idle
Accepting Yes
</Printer>
<Printer SGLabel_Windows>
Info SGLabel (maker's driver)
MakeModel SGLabel (maker's driver)
DeviceURI file:///dev/null
State Idle
Accepting Yes
</Printer>
EOF
ppd "SG Test UMPD" > "$T/root/ppd/SGLabel.ppd"
ppd "SGLabel (maker's driver)" > "$T/root/ppd/SGLabel_Windows.ppd"
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
lpstat -v 2>/dev/null | grep -q SGLabel || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

printf 'LIBRARY gdi32.dll\nEXPORTS\nEngCreateBitmap\nEngAssociateSurface\nEngDeleteSurface\nEngWritePrinter\n' > "$T/eng.def"
"${MINGW%-gcc}-dlltool" -d "$T/eng.def" -l "$T/libeng.a" || { echo "SKIP: no dlltool"; exit 77; }
"$MINGW" -shared -O2 -o "$T/sgtestumpd.dll" "$HERE/printdrv-umpd.c" "$T/libeng.a" -lgdi32 -lwinspool || { echo "FAIL  driver did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/"
run() { (cd "$C" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }

out=$(run 'C:\probe64.exe' enum | grep '^enum SGLabel' | tr '\n' ';')
[ "$out" = "enum SGLabel|SG Test UMPD|CUPS:SGLabel|wineps;" ] &&
    pass "without the maker's package the queue prints with the PostScript driver" ||
    fail "without the package: $out"

# the maker's package arrives in the driver store
S="$C/windows/system32/DriverStore/FileRepository/sgtestumpd.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgtestumpd.dll" "$S/amd64/"
printf 'SG test data\r\n' > "$S/sgtest.dat"
cat > "$S/sgtestumpd.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
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
"$WINESERVER" -w
out=$(run 'C:\probe64.exe' enum | grep '^enum SGLabel' | tr '\n' ';')
[ "$out" = "enum SGLabel|SG Test UMPD|CUPS:SGLabel|winprint;" ] &&
    pass "with the maker's package the queue's printer uses its Windows driver" ||
    fail "with the package: $out"
case "$out" in *SGLabel_Windows*) fail "the Windows-driver queue for Linux programs is a Windows printer" ;;
    *) pass "the queue published for Linux programs is not a Windows printer" ;; esac

out=$(run 'C:\probe64.exe' print SGLabel)
sleep 2
job=$(ls "$T/spool"/d*-001 2>/dev/null | tail -1)
type=$(grep -o 'File of type [^ ]* queued' "$T/log/error_log" 2>/dev/null | tail -1)
first=$(head -1 "$job" 2>/dev/null | tr -d '\r')
if [ "$out" = "print ok" ] && [ "$type" = "File of type application/vnd.cups-raw queued" ] &&
   [ "$first" = "SGTD START printdrv probe" ]; then
    pass "the driver's output reached CUPS as a raw job"
else
    fail "the CUPS job: '$out', '$type', first line '$first'"
fi

# the Linux driver chosen for the printer
"$WINESERVER" -w
timeout 60 "$WINE" reg add 'HKLM\System\CurrentControlSet\Control\Print\Printers\SGLabel' /v SgPrinterDriver /d linux /f >/dev/null 2>&1
"$WINESERVER" -w
out=$(run 'C:\probe64.exe' enum | grep '^enum SGLabel' | tr '\n' ';')
[ "$out" = "enum SGLabel|SG Test UMPD (CUPS)|CUPS:SGLabel|wineps;" ] &&
    pass "the Linux driver chosen: the PostScript driver, beside the Windows one" ||
    fail "the Linux driver chosen: $out"

[ $RC = 0 ] && echo "cupsdrv gate: PASS" || echo "cupsdrv gate: FAIL"
exit $RC
