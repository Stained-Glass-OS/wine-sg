#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A printer's properties open on Windows' General page (patches/sg/9816):
# its name, location, comment and model, what it can do (colour,
# double-sided, staple, speed, maximum resolution, the paper it takes) and
# the Preferences... and Print Test Page buttons, before the driver's pages.
# They had only the driver's Device Settings page. The gate runs a CUPS
# server of its own with a stopped colour, double-sided queue (a PPD of its
# own: Letter and A4) and opens PrinterProperties from a Windows program: the
# first page is General with the printer's name, model, Yes for colour and
# double-sided, its two papers; Print Test Page puts a document in the
# printer's queue and says so.
#
#   WINE=/opt/wine-sg/bin/wine test/printprops-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
LPADMIN="${LPADMIN:-/usr/sbin/lpadmin}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$LPADMIN" ] || { echo "SKIP: no lpadmin"; exit 77; }
command -v cupsdisable >/dev/null || { echo "SKIP: no cupsdisable"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-printprops.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
cleanup() {
    "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"
    [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
cat > "$T/root/cupsd.conf" <<EOF
Listen $T/cups.sock
LogLevel warn
PreserveJobFiles No
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
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
[ -S "$T/cups.sock" ] || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

cat > "$T/laser.ppd" <<'EOPPD'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGLASER.PPD"
*Manufacturer: "Stained Glass"
*Product: "(SG Office Laser)"
*ModelName: "SG Office Laser"
*ShortNickName: "SG Office Laser"
*NickName: "SG Office Laser"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*FileSystem: False
*Throughput: "20"
*LandscapeOrientation: Plus90
*TTRasterizer: Type42
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 10 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*PageSize A4/A4: "<</PageSize[595 842]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
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
*OpenUI *Duplex/Two-Sided Printing: PickOne
*OrderDependency: 20 AnySetup *Duplex
*DefaultDuplex: None
*Duplex None/Off: "<</Duplex false>>setpagedevice"
*Duplex DuplexNoTumble/Long Edge: "<</Duplex true/Tumble false>>setpagedevice"
*Duplex DuplexTumble/Short Edge: "<</Duplex true/Tumble true>>setpagedevice"
*CloseUI: *Duplex
*DefaultResolution: 600dpi
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOPPD
"$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/printprops-probe.c" -lwinspool -lcomctl32 || { echo "FAIL  probe did not build"; exit 1; }
"$LPADMIN" -p SG_Laser -E -v file:///dev/null -P "$T/laser.ppd" -D "Office Laser" 2>/dev/null
cupsdisable SG_Laser 2>/dev/null
lpstat -p SG_Laser 2>/dev/null | grep -q disabled || { echo "SKIP: the gate's CUPS server did not take the queue"; exit 77; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\\probe.exe' SG_Laser 2>"$T/probe.err" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
sed 's/^/      /' "$T/probe.out"
if grep -q '^RESULT: PASS' "$T/probe.out"; then echo "PASS  the printer's properties open on the General page"; echo "RESULT: PASS"; exit 0; fi
echo "FAIL  the printer's properties' General page"; tail -5 "$T/probe.err"; echo "RESULT: FAIL"; exit 1
