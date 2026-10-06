#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A USB label printer as its maker's software reaches it (patches/sg/0874-
# 0878), without the printer: the gate builds a libusb-1.0 of its own with one
# USB printer on it (test/usbprint-fakeusb.c: 1209:0001, printer class,
# bidirectional, an IEEE 1284 ID, a 32-byte status answering ESC A, the
# interface held by a kernel driver as usblp holds a printer's) and puts it
# first on LD_LIBRARY_PATH, so wineusb.sys meets it. test/usbprint-probe.c
# then does what DYMO Connect does with a LabelWriter 550, 64- and 32-bit:
#
#   0874 usbprint.sys: GUID_DEVINTERFACE_USBPRINT, the 1284 ID and port
#        status ioctls, ESC A 0 written on one overlapped handle and the
#        status read on another; wineusb claims the interface from the kernel
#        driver for the while and gives it back when the last handle closes
#   0875 setupapi: the USB device's hardware IDs (CM_Get_DevNode_Registry_
#        Property) and the Printer-class device whose parent is it (CM_Get_Parent)
#   0876 winspool: the CUPS queue on usb://...?serial=SG0001 names that device
#        (PnPData DeviceInstanceId), and a printer missing from this user's
#        [Devices] (added by another user: SYSTEM) is put there, so
#        CreateDC(NULL, queue) finds it
#   0877 kernelbase: the user's country by name (GEO_NAME, GEO_FRIENDLYNAME)
#   0878 prntvpt, wineps: a print ticket's custom size (a label's printable
#        area, landscape) prints on the PPD's page of that label
#
#   WINE=/opt/wine-sg/bin/wine test/usbprint-gate.sh
# Mutation (each makes its check fail): -DSG_MUTANT_USBCLAIM (wineusb.sys),
# -DSG_MUTANT_USBPRINT_SHARE (usbprint.sys), -DSG_MUTANT_CMDEVNODEPROP and
# -DSG_MUTANT_CMGETPARENT (setupapi, ntoskrnl), -DSG_MUTANT_PNPDATA and
# -DSG_MUTANT_USERDEVICES (winspool), -DSG_MUTANT_GEONAME (kernelbase),
# -DSG_MUTANT_PAPERMATCH (prntvpt, wineps).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
LPADMIN="${LPADMIN:-/usr/sbin/lpadmin}"
for t in "$MINGW" "$MINGW32" cc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/libusb-1.0/libusb.h ] || { echo "SKIP: libusb-1.0-0-dev missing"; exit 77; }
[ -x "$CUPSD" ] && [ -x "$LPADMIN" ] || { echo "SKIP: no cupsd/lpadmin"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-usbprint.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=${SG_WINEDEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
export LD_LIBRARY_PATH="$T/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" SG_FAKEUSB_LOG="$T/written.bin" SG_FAKEUSB_TRACE="$T/usb.trace"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/lib" "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
cc -O2 -shared -fPIC -Wl,-soname,libusb-1.0.so.0 -o "$T/lib/libusb-1.0.so.0" "$HERE/usbprint-fakeusb.c" -lpthread ||
    { echo "FAIL  the simulated printer did not build"; exit 1; }
for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/usbprint-probe.c" \
        -lsetupapi -lcfgmgr32 -lwinspool -lprntvpt -lole32 -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
done

# a CUPS server of the gate's own, with the printer's queue on its USB URI
cat > "$T/root/cupsd.conf" <<EOF
Listen $T/cups.sock
LogLevel warn
DirtyCleanInterval 0
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
ServerBin $T/serverbin
DataDir /usr/share/cups
FileDevice Yes
User $(id -un)
Group $(id -gn)
EOF
: > "$T/root/printers.conf"
# CUPS's own programs, but a usb backend that is the gate's: the system's is
# root's, and a queue's URI scheme must name a backend cupsd can run
mkdir -p "$T/serverbin/backend"
for d in cgi-bin daemon driver filter monitor notifier; do
    [ -e "/usr/lib/cups/$d" ] && ln -s "/usr/lib/cups/$d" "$T/serverbin/$d"
done
printf '#!/bin/sh\n[ $# = 0 ] && echo "direct usb \\"Unknown\\" \\"USB Printer (gate)\\""\nexit 0\n' > "$T/serverbin/backend/usb"
chmod 755 "$T/serverbin/backend/usb"
# a label printer's PPD: the default page is a shipping label, the 1 x 2-1/8
# in label is another, and landscape turns the page clockwise (as DYMO's)
cat > "$T/label.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGLABEL.PPD"
*Manufacturer: "Stained Glass"
*Product: "(Test Label Printer)"
*ModelName: "Stained Glass Test Label Printer"
*ShortNickName: "Stained Glass Test Label Printer"
*NickName: "Stained Glass Test Label Printer"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: False
*DefaultColorSpace: Gray
*FileSystem: False
*Throughput: "1"
*LandscapeOrientation: Minus90
*TTRasterizer: Type42
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 10 AnySetup *PageSize
*DefaultPageSize: w167h288
*PageSize w167h288/Shipping 2-5/16 in x 4 in: "<</PageSize[167 288]/ImagingBBox null>>setpagedevice"
*PageSize w72h154/Multipurpose 1 in x 2-1/8 in: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
*DefaultPageRegion: w167h288
*PageRegion w167h288/Shipping 2-5/16 in x 4 in: "<</PageSize[167 288]/ImagingBBox null>>setpagedevice"
*PageRegion w72h154/Multipurpose 1 in x 2-1/8 in: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: w167h288
*ImageableArea w167h288/Shipping 2-5/16 in x 4 in: "4.08 4.32 163.20 271.20"
*ImageableArea w72h154/Multipurpose 1 in x 2-1/8 in: "4.08 4.32 69.12 146.64"
*DefaultPaperDimension: w167h288
*PaperDimension w167h288/Shipping 2-5/16 in x 4 in: "167.04 288.00"
*PaperDimension w72h154/Multipurpose 1 in x 2-1/8 in: "72.00 153.12"
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
EOF
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
[ -S "$T/cups.sock" ] || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }
Q=Test_Label_Printer
"$LPADMIN" -p $Q -E -v 'usb://Stained%20Glass/Test%20Label%20Printer?serial=SG0001' -P "$T/label.ppd" 2>"$T/lpadmin.err"
lpstat -v $Q >/dev/null 2>&1 || { echo "SKIP: the gate's CUPS server did not take the queue: $(cat "$T/lpadmin.err")"; exit 77; }
# the queue on disk before Wine starts: winspool remakes the printers when
# CUPS's printers.conf changes (0873), and a remake mid-test is not the test
i=0; while ! grep -q "<Printer $Q>" "$T/root/printers.conf" 2>/dev/null && [ $i -lt 100 ]; do sleep 0.2; i=$((i + 1)); done

# the prefix's server, and the drivers it starts, stay up for the whole
# test: started by a probe, they would hold the probe's output open
mkdir -p "$WINEPREFIX"
"$WINESERVER" -p >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" "$@" > "$T/run.out" 2>"$T/run.err"); [ -n "${SG_WINEDEBUG:-}" ] && grep -v "^$" "$T/run.err" | tail -n 40 >&2; tr -d '\r' < "$T/run.out"; }
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -n 1; }

for b in 64 32; do
    : > "$T/usb.trace"
    # wineusb.sys lists the device when it comes up, a moment after the boot
    i=0; while out=$(run "C:\\probe$b.exe" usb); [ "$(v iface)" = none ] && [ $i -lt 30 ]; do sleep 1; i=$((i + 1)); done
    printf '%s\n' "$out" | sed 's/^/      /'
    case "$(v iface)" in *"vid_1209&pid_0001"*"{28d78fad-5a12-11d1-ae5b-0000f803a8c2}") pass "$b-bit: the printer is a USBPRINT interface" ;;
        *) fail "$b-bit: no USBPRINT interface: '$(v iface)'"; continue ;; esac
    [ "$(v id)" = "MFG:Stained Glass;MDL:Test Label Printer;CMD:ESC;CLS:PRINTER;SN:SG0001;" ] &&
        pass "$b-bit: its IEEE 1284 ID" || fail "$b-bit: 1284 ID '$(v id)'"
    [ "$(v lpt)" = 0x18 ] && pass "$b-bit: its port status" || fail "$b-bit: port status '$(v lpt)'"
    if [ "$(v wrote)" = 3 ] && [ "$(v roll)" = 30336 ]; then
        pass "$b-bit: ESC A written on one handle, the status read on another: roll $(v roll)"
    else
        fail "$b-bit: status request: wrote '$(v wrote)', reply '$(v reply)'"
    fi
    grep -q 'refused' "$T/usb.trace" && fail "$b-bit: the kernel driver kept the interface: $(grep refused "$T/usb.trace" | head -n 1)"
    [ "$(grep -E '^(claim|release)' "$T/usb.trace" | tail -n 1)" = "release ok" ] &&
        pass "$b-bit: the interface given back when the last handle closed" ||
        fail "$b-bit: the interface still claimed after the program closed the printer"
    [ "$(v hwid)" = 'USB\VID_1209&PID_0001&REV_0100' ] &&
        pass "$b-bit: CM_Get_DevNode_Registry_Property: $(v hwid)" || fail "$b-bit: hardware ID '$(v hwid)'"
    [ "$(v printerdev)" = 'USBPRINT\STAINED_GLASSTEST_LA\SG0001' ] &&
        pass "$b-bit: the Printer-class device $(v printerdev)" || fail "$b-bit: Printer-class device '$(v printerdev)'"
    [ -n "$(v usbdev)" ] && [ "$(v parent)" = "$(v usbdev)" ] &&
        pass "$b-bit: CM_Get_Parent of it is the USB device" || fail "$b-bit: parent '$(v parent)', USB device '$(v usbdev)'"

    out=$(run "C:\\probe$b.exe" pnp $Q)
    [ "$(v DeviceInstanceId)" = 'USBPRINT\STAINED_GLASSTEST_LA\SG0001' ] &&
        pass "$b-bit: the queue's PnPData names its device" || fail "$b-bit: PnPData: $out"

    out=$(run "C:\\probe$b.exe" geo)
    [ "$(v name)" = US ] && [ "$(v friendly)" = "United States" ] &&
        pass "$b-bit: GetGeoInfo: $(v name), $(v friendly)" || fail "$b-bit: geo: $out"
done
[ -s "$T/written.bin" ] && pass "the printer received $(wc -c < "$T/written.bin") bytes" || fail "nothing reached the printer"

# a printer another user added: missing from this user's [Devices]
"$WINE" reg delete 'HKCU\Software\Microsoft\Windows NT\CurrentVersion\Devices' /v $Q /f >/dev/null 2>&1
"$WINE" reg delete 'HKCU\Software\Microsoft\Windows NT\CurrentVersion\PrinterPorts' /v $Q /f >/dev/null 2>&1
out=$(run 'C:\probe64.exe' devices $Q)
case "$(v devices)" in wineps.drv,*) pass "a printer missing from this user's [Devices] is put there: $(v devices)" ;;
    *) fail "the printer is not in this user's [Devices]: $out" ;; esac

for b in 64 32; do
    rm -f "$WINEPREFIX/drive_c/label.ps"
    out=$(run "C:\\probe$b.exe" dc $Q 'C:\label.ps')
    printf '%s\n' "$out" | sed 's/^/      /'
    case "$(v ticket)" in *"paper -1 width 229 length 502"*) pass "$b-bit: the ticket's custom size is a width and length" ;;
        *) fail "$b-bit: ticket devmode '$(v ticket)'" ;; esac
    case "$(v merged)" in *"width 254 length 540"*) pass "$b-bit: the driver took the label's page for it" ;;
        *) fail "$b-bit: merged devmode '$(v merged)'" ;; esac
    ps="$WINEPREFIX/drive_c/label.ps"
    if [ "$(v printed)" = ok ] && grep -q '^%%BeginFeature: \*PageSize w72h154$' "$ps" && grep -q '^%cupsJobTicket: media=w72h154$' "$ps" &&
       grep -q '^%%PageOrientation: Landscape' "$ps"; then
        pass "$b-bit: printed on the label's page (PageSize w72h154, landscape)"
    else
        fail "$b-bit: the page printed: $(grep -a -m3 'PageSize\|media=\|PageOrientation' "$ps" 2>/dev/null | tr '\n' ' ') $out"
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
