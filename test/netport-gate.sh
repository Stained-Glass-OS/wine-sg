#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Printers on the network through a Windows driver, and what the printer
# says back (patches/sg/1029): printers of our own on 127.0.0.1
# (test/netport-server.py: raw TCP, LPR, IPP) and a PostScript package
# with a language monitor of our own (test/langmon-monitor.c):
#   - Standard TCP/IP ports are made through the monitor's Xcv interface
#     (AddPort, GetConfigInfo, DeletePort) and listed by EnumPorts;
#   - a job reaches the raw printer whole, the LPR printer by RFC 1179 in
#     its queue, the IPP printer by a Print-Job to the printer's URI;
#   - the job goes through the driver's language monitor (OpenPortEx with
#     the port monitor), which wraps it;
#   - what the monitor tells the system (SetPort: toner low) is in the
#     printer's status (GetPrinter).
#   WINE=/opt/wine-sg/bin/wine test/netport-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-netport.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export CUPS_SERVER="$T/no-cups.sock"
mkdir -p "$T/net"
python3 "$HERE/netport-server.py" "$T/net" >"$T/server.log" 2>&1 & SP=$!
trap '"$WINESERVER" -k 2>/dev/null; kill $SP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

"$MINGW" -municode -O2 -o "$T/netport.exe" "$HERE/netport-probe.c" -lwinspool || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/sgp64.exe" "$HERE/../tools/printer-corpus/sgprint.c" -lwinspool -lgdi32 -lsetupapi ||
    { echo "FAIL  sgprint did not build"; exit 1; }
"$MINGW" -shared -O2 -o "$T/sglm.dll" "$HERE/langmon-monitor.c" -lwinspool || { echo "FAIL  monitor did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/netport.exe" "$T/sgp64.exe" "$C/"
for i in $(seq 50); do [ -s "$T/net/ports" ] && break; sleep 0.1; done
RAW=$(awk '/^raw/{print $2}' "$T/net/ports"); LPR=$(awk '/^lpr/{print $2}' "$T/net/ports"); IPP=$(awk '/^ipp/{print $2}' "$T/net/ports")

S="$C/windows/system32/DriverStore/FileRepository/sgnet.inf_1"
mkdir -p "$S"
cp "$T/sglm.dll" "$S/"
cat > "$S/sgnet.ppd" <<'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGNET.PPD"
*Manufacturer: "Stained Glass OS"
*ModelName: "SG Net PS"
*NickName: "SG Net PS"
*ShortNickName: "SG Net PS"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: False
*DefaultColorSpace: Gray
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
cat > "$S/sgnet.inf" <<'EOF'
[Version]
Signature="$Windows NT$"
Provider=Stained Glass OS
ClassGUID={4D36E979-E325-11CE-BFC1-08002BE10318}
Class=Printer

[Manufacturer]
"SG Test" = SGTEST, NTamd64

[SGTEST.NTamd64]
"SG Net PS" = SGNET_INSTALL

[SGNET_INSTALL]
CopyFiles=@sgnet.ppd, SGNET_LM
DataFile=sgnet.ppd
LanguageMonitor="SG Test Language Monitor,sglm.dll"
Include=NTPRINT.INF
Needs=PSCRIPT.OEM

[SGNET_LM]
sglm.dll

[DestinationDirs]
DefaultDestDir=66000
SGNET_LM=66002

[SourceDisksNames]
1 = "SG test disk",,,

[SourceDisksFiles]
sgnet.ppd = 1
sglm.dll = 1
EOF

run() { (cd "$C" && timeout 120 "$WINE" "$@" </dev/null 2>>"$T/stderr.log" | tr -d '\r'); }
out=$(run 'C:\sgp64.exe' install "SG Net PS")
[ "$out" = "install 0" ] && pass "the package with a language monitor installs" || fail "install: $out"
"$WINESERVER" -w

out=$(run 'C:\netport.exe' add IP_SGRAW raw 127.0.0.1 "$RAW")
[ "$out" = "addport 0" ] && pass "AddPort through the TCP/IP monitor's Xcv interface" || fail "AddPort raw: $out"
out=$(run 'C:\netport.exe' add IP_SGLPR lpr 127.0.0.1 "$LPR" sgqueue)
[ "$out" = "addport 0" ] && pass "AddPort for an LPR printer" || fail "AddPort lpr: $out"
out=$(run 'C:\netport.exe' get IP_SGLPR)
[ "$out" = "config IP_SGLPR protocol 2 host 127.0.0.1 port $LPR queue sgqueue" ] &&
    pass "GetConfigInfo reads the port back" || fail "GetConfigInfo: $out"
out=$(run 'C:\netport.exe' ports | grep "IP_SG" | tr '\n' ';')
[ "$out" = "port IP_SGLPR|Standard TCP/IP Port;port IP_SGRAW|Standard TCP/IP Port;" ] &&
    pass "EnumPorts lists them with their monitor" || fail "EnumPorts: $out"
run 'C:\netport.exe' add IP_SGGONE raw 127.0.0.1 1 >/dev/null
out=$(run 'C:\netport.exe' delete IP_SGGONE)
left=$(run 'C:\netport.exe' ports | grep -c "IP_SGGONE")
[ "$out" = "deleteport 0" ] && [ "$left" = 0 ] && pass "DeletePort" || fail "DeletePort: $out, left $left"

run 'C:\sgp64.exe' add "Net Raw" "SG Net PS" IP_SGRAW >/dev/null
run 'C:\sgp64.exe' add "Net LPR" "SG Net PS" IP_SGLPR >/dev/null
run 'C:\sgp64.exe' add "Net IPP" "SG Net PS" "ipp://127.0.0.1:$IPP/ipp/print" >/dev/null

out=$(run 'C:\sgp64.exe' print "Net Raw")
case "$out" in "print ok"*) pass "a job to the raw printer" ;; *) fail "raw print: $out" ;; esac
head -1 "$T/net/raw.bin" 2>/dev/null | grep -q "^<SGLM job [0-9]*>$" && pass "the job went through the language monitor" ||
    fail "no language monitor wrap: $(head -c 30 "$T/net/raw.bin" 2>/dev/null | od -c | head -1)"
grep -a -q '%!PS-Adobe' "$T/net/raw.bin" 2>/dev/null && tail -c 9 "$T/net/raw.bin" | grep -q "</SGLM>" &&
    pass "the raw printer got the whole PostScript job" || fail "the raw printer's data"
out=$(run 'C:\sgp64.exe' status "Net Raw")
case "$out" in *toner_low*|*"0x20000"*|*TONER*) pass "the language monitor's state is the printer's status ($out)" ;;
     *) fail "printer status: $out" ;; esac

out=$(run 'C:\sgp64.exe' print "Net LPR")
case "$out" in "print ok"*) pass "a job to the LPR printer" ;; *) fail "lpr print: $out" ;; esac
[ "$(cat "$T/net/lpr-queue.txt" 2>/dev/null)" = sgqueue ] && pass "LPR: the job is in the port's queue" ||
    fail "LPR queue: $(cat "$T/net/lpr-queue.txt" 2>/dev/null)"
grep -q "^ldfA" "$T/net/lpr-control.txt" 2>/dev/null && grep -q "^JSG printer test" "$T/net/lpr-control.txt" 2>/dev/null &&
    pass "LPR: the control file names the data file and the job" ||
    fail "LPR control file: $(tr '\n' '|' < "$T/net/lpr-control.txt" 2>/dev/null)"
grep -a -q '%!PS-Adobe' "$T/net/lpr-data.bin" 2>/dev/null && pass "LPR: the data file is the job" || fail "LPR data"

out=$(run 'C:\sgp64.exe' print "Net IPP")
case "$out" in "print ok"*) pass "a job to the IPP printer" ;; *) fail "ipp print: $out" ;; esac
grep -qx "version=101 op=2" "$T/net/ipp-request.txt" 2>/dev/null && grep -qx "printer-uri=ipp://127.0.0.1:$IPP/ipp/print" "$T/net/ipp-request.txt" &&
    grep -qx "document-format=application/octet-stream" "$T/net/ipp-request.txt" &&
    pass "IPP: a Print-Job to the printer's URI" || fail "IPP request: $(tr '\n' '|' < "$T/net/ipp-request.txt" 2>/dev/null)"
grep -a -q '%!PS-Adobe' "$T/net/ipp-data.bin" 2>/dev/null && pass "IPP: the document is the job" || fail "IPP data"

if [ $RC != 0 ]; then echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme" | tail -15; fi
[ $RC = 0 ] && echo "netport gate: PASS" || echo "netport gate: FAIL"
exit $RC
