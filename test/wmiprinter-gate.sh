#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Win32_Printer lists the printers (patches/sg/0870).
# wbemprox asked EnumPrinters for the size of the list and took the count it
# reported -- which a sizing call leaves at 0, on Windows as in Wine -- for the
# number of printers: SELECT * FROM Win32_Printer was always empty. DYMO
# Connect finds its label printers that way and said "No label printer found"
# however the printer was set up. The probe adds a printer whose driver is
# "DYMO LabelWriter 550" and asks WMI (64- and 32-bit) for it.
#
#   WINE=/opt/wine-sg/bin/wine test/wmiprinter-gate.sh
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
T=$(mktemp -d /var/tmp/sg-wmiprinter.XXXXXX)
# no CUPS: only the printer the probe adds
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/wmiprinter-probe.c" -lwinspool -lole32 -loleaut32 -luuid -lwbemuuid ||
        { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
cat > "$WINEPREFIX/drive_c/label.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGLABEL.PPD"
*Manufacturer: "SG"
*ModelName: "SG Label"
*NickName: "SG Label"
*ColorDevice: False
*DefaultColorSpace: Gray
*OpenUI *PageSize: PickOne
*DefaultPageSize: w72h154
*PageSize w72h154/Address: "<</PageSize[72 154]>>setpagedevice"
*CloseUI: *PageSize
*DefaultImageableArea: w72h154
*ImageableArea w72h154/Address: "4 4 68 150"
*DefaultPaperDimension: w72h154
*PaperDimension w72h154/Address: "72 154"
EOF
run() { (cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
add=$(run 'C:\probe64.exe' add "LabelWriter on desk" "DYMO LabelWriter 550" "USB001" 'C:\label.ppd')
case "$add" in added=1*) ;; *) echo "FAIL  the test printer was not added ($add)"; exit 1 ;; esac
for b in 64 32; do
    out=$(run "C:\\probe$b.exe" wmi)
    printf '      %s\n' "$out"
    if printf '%s\n' "$out" | grep -qx 'wmi LabelWriter on desk|DYMO LabelWriter 550|USB001'; then
        pass "$b-bit: Win32_Printer lists the printer with its driver and port"
    else
        fail "$b-bit: Win32_Printer does not list the printer (DYMO Connect: \"No label printer found\")"
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
