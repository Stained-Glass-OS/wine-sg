#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A printer added to CUPS during a session reaches Windows programs
# (patches/sg/0873). The desktop keeps winspool loaded (0609), so the Windows
# printers were made from CUPS's once a session: a LabelWriter plugged in
# later (sg-session's sg-dymo-queue makes its queue then) reached no Windows
# program -- DYMO Connect -- before the next sign-in. The gate runs a CUPS
# server of its own with no printer, keeps winspool loaded in one program as
# the desktop does, adds a queue for a DYMO LabelWriter 550 with lpadmin, and
# lists the printers in a new program, 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/cupsrefresh-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-cupsrefresh.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
cat > "$T/root/cupsd.conf" <<EOF
Listen $T/cups.sock
LogLevel warn
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
FileDevice Yes
User $(id -un)
Group $(id -gn)
EOF
: > "$T/root/printers.conf"
cat > "$T/lw550.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGLW550.PPD"
*Manufacturer: "DYMO"
*Product: "(DYMO LabelWriter 550)"
*ModelName: "DYMO LabelWriter 550"
*ShortNickName: "DYMO LabelWriter 550"
*NickName: "DYMO LabelWriter 550"
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
*DefaultPageSize: w72h154
*PageSize w72h154/30252 Address: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
*DefaultPageRegion: w72h154
*PageRegion w72h154/30252 Address: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: w72h154
*ImageableArea w72h154/30252 Address: "4 4 68 150"
*DefaultPaperDimension: w72h154
*PaperDimension w72h154/30252 Address: "72 154"
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
[ -S "$T/cups.sock" ] || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }
LPADMIN="${LPADMIN:-/usr/sbin/lpadmin}"
[ -x "$LPADMIN" ] || { echo "SKIP: no lpadmin"; exit 77; }

for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/wmiprinter-probe.c" -lwinspool -lole32 -loleaut32 -luuid -lwbemuuid ||
        { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
# the desktop: winspool loaded for the whole test
(cd "$WINEPREFIX/drive_c" && timeout 300 "$WINE" 'C:\probe64.exe' hold 240 > "$T/hold.out" 2>/dev/null &)
i=0; while ! grep -q holding "$T/hold.out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
out=$(run 'C:\probe64.exe' enum)
case "$out" in *"count=0"*) ;; *) echo "FAIL  a printer before any was added: $out"; exit 1 ;; esac
sleep 1.1
"$LPADMIN" -p DYMO_LabelWriter_550 -E -v file:///dev/null -P "$T/lw550.ppd" 2>/dev/null
lpstat -v DYMO_LabelWriter_550 >/dev/null 2>&1 || { echo "SKIP: the gate's CUPS server did not take the queue"; exit 77; }
for b in 64 32; do
    out=$(run "C:\\probe$b.exe" enum)
    printf '      %s\n' "$out"
    if printf '%s\n' "$out" | grep -qx 'enum DYMO_LabelWriter_550|DYMO LabelWriter 550|CUPS:DYMO_LabelWriter_550'; then
        pass "$b-bit: the queue added during the session is a Windows printer, without signing out"
    else
        fail "$b-bit: the queue added during the session is not a Windows printer"
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
