#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A printer's PrintCapabilities are its driver's (patches/sg/1221), and a
# printer CUPS already has is a Windows printer in a brand-new prefix, with
# its queue's status.
#
# PTGetPrintCapabilities wrote every feature empty, so a program choosing its
# page from the capabilities found no paper: DYMO Connect, which looks for its
# label among the PageMediaSize options, printed on a "custom" page the driver
# did not have; WPF's PrintDialog had no sizes, trays or duplex to offer. The
# gate runs a CUPS server of its own with a queue whose PPD has labels and
# letter/A4 pages, three input slots, duplex and two resolutions, then, 64-
# and 32-bit:
#   - the queue is a Windows printer in a prefix made after it (a printer
#     CUPS had when the prefix was built was missing until CUPS changed;
#     fixed by 0924, kept here);
#   - the capabilities list every paper with its name and size (the driver's
#     own ones in our namespace), a custom size's range, the imageable area,
#     the bins (psk:Manual, ours), duplex, resolutions, orientations, copies;
#   - a ticket naming a paper option from the capabilities, as DYMO Connect
#     writes it, becomes that paper in the DEVMODE; a custom size the driver
#     has a paper of becomes that paper;
#   - the printer's status is its queue's (1222): paused with cupsdisable,
#     ready again with cupsenable (every CUPS printer was always "ready").
#
#   WINE=/opt/wine-sg/bin/wine test/ptcaps-gate.sh
# Mutation (each fails it): -DSG_MUTANT_PTCAPS, -DSG_MUTANT_PTPAPERNAME in
# dlls/prntvpt/ticket.c, -DSG_MUTANT_PRNSTATUS in dlls/winspool.drv/info.c.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
for t in "$MINGW" "$MINGW32" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-ptcaps.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
# no display: not the session's Wayland compositor either (Wine's Wayland
# driver connects to $XDG_RUNTIME_DIR/wayland-0 when WAYLAND_DISPLAY is unset)
mkdir -p -m 700 "$T/xdg"; export XDG_RUNTIME_DIR="$T/xdg"
cat > "$T/root/cupsd.conf" <<CONF
Listen $T/cups.sock
LogLevel warn
<Location />
Order allow,deny
Allow all
</Location>
CONF
cat > "$T/root/cups-files.conf" <<CONF
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
CONF
cat > "$T/root/printers.conf" <<CONF
<Printer SG_Office_Printer>
Info Office printer
MakeModel SG Office Printer
DeviceURI file:///dev/null
State Idle
Accepting Yes
</Printer>
CONF
cat > "$T/root/ppd/SG_Office_Printer.ppd" <<'PPD'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "SGOFFICE.PPD"
*Manufacturer: "Stained Glass"
*Product: "(SG Office Printer)"
*ModelName: "SG Office Printer"
*ShortNickName: "SG Office Printer"
*NickName: "SG Office Printer"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*FileSystem: False
*Throughput: "20"
*LandscapeOrientation: Plus90
*TTRasterizer: Type42
*VariablePaperSize: True
*MaxMediaWidth: "612"
*MaxMediaHeight: "1008"
*HWMargins: 0 0 0 0
*ParamCustomPageSize Width: 1 points 36 612
*ParamCustomPageSize Height: 2 points 72 1008
*ParamCustomPageSize WidthOffset: 3 points 0 0
*ParamCustomPageSize HeightOffset: 4 points 0 0
*ParamCustomPageSize Orientation: 5 int 0 0
*CustomPageSize True: "pop pop pop <</PageSize[5 -2 roll]/ImagingBBox null>>setpagedevice"
*OpenUI *PageSize/Media Size: PickOne
*OrderDependency: 10 AnySetup *PageSize
*DefaultPageSize: Letter
*PageSize Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*PageSize A4/A4: "<</PageSize[595 842]/ImagingBBox null>>setpagedevice"
*PageSize w72h154/30336 1 in x 2-1/8 in: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*PageSize w79h252/30252 Address: "<</PageSize[79 252]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*OrderDependency: 10 AnySetup *PageRegion
*DefaultPageRegion: Letter
*PageRegion Letter/Letter: "<</PageSize[612 792]/ImagingBBox null>>setpagedevice"
*PageRegion A4/A4: "<</PageSize[595 842]/ImagingBBox null>>setpagedevice"
*PageRegion w72h154/30336 1 in x 2-1/8 in: "<</PageSize[72 154]/ImagingBBox null>>setpagedevice"
*PageRegion w79h252/30252 Address: "<</PageSize[79 252]/ImagingBBox null>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/Letter: "18 18 594 774"
*ImageableArea A4/A4: "18 18 577 824"
*ImageableArea w72h154/30336 1 in x 2-1/8 in: "4 4 68 150"
*ImageableArea w79h252/30252 Address: "4 4 75 248"
*DefaultPaperDimension: Letter
*PaperDimension Letter/Letter: "612 792"
*PaperDimension A4/A4: "595 842"
*PaperDimension w72h154/30336 1 in x 2-1/8 in: "72 154"
*PaperDimension w79h252/30252 Address: "79 252"
*OpenUI *InputSlot/Media Source: PickOne
*OrderDependency: 20 AnySetup *InputSlot
*DefaultInputSlot: Auto
*InputSlot Auto/Automatically Select: "<</ManualFeed false>>setpagedevice"
*InputSlot Manual/Manual Feed: "<</ManualFeed true>>setpagedevice"
*InputSlot Tray2/Tray 2: "<</MediaPosition 2>>setpagedevice"
*CloseUI: *InputSlot
*OpenUI *Duplex/2-Sided Printing: PickOne
*OrderDependency: 30 AnySetup *Duplex
*DefaultDuplex: None
*Duplex None/Off: "<</Duplex false>>setpagedevice"
*Duplex DuplexNoTumble/Long-Edge: "<</Duplex true/Tumble false>>setpagedevice"
*Duplex DuplexTumble/Short-Edge: "<</Duplex true/Tumble true>>setpagedevice"
*CloseUI: *Duplex
*OpenUI *Resolution/Resolution: PickOne
*OrderDependency: 40 AnySetup *Resolution
*DefaultResolution: 600dpi
*Resolution 300dpi/300 dpi: "<</HWResolution[300 300]>>setpagedevice"
*Resolution 600dpi/600 dpi: "<</HWResolution[600 600]>>setpagedevice"
*CloseUI: *Resolution
*DefaultFont: Courier
*Font Courier: Standard "(002.004S)" Standard ROM
PPD
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
lpstat -v 2>/dev/null | grep -q SG_Office_Printer || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/ptcaps-probe.c" -lwinspool -lprntvpt -lole32 ||
        { echo "FAIL  probe did not build"; exit 1; }
done
# the prefix is made while the queue exists, its server and services staying
# up (as a session's desktop keeps them)
mkdir -p "$WINEPREFIX"
"$WINESERVER" -p >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
P=SG_Office_Printer
for b in 64 32; do
    dc=$(run "C:\\probe$b.exe" devcaps $P)
    if printf '%s\n' "$dc" | grep -q '^paper .*|30336 1 in x 2-1/8 in|'; then
        pass "$b-bit: the queue CUPS had when the prefix was made is a Windows printer, with its PPD's papers"
    else
        fail "$b-bit: the queue is not a Windows printer of the new prefix: $(printf '%s' "$dc" | head -3 | tr '\n' ' ')"
        continue
    fi
    printf '%s\n' "$dc" > "$T/dc$b.txt"
    run "C:\\probe$b.exe" caps $P > "$T/caps$b.xml"
    python3 - "$T/caps$b.xml" "$T/dc$b.txt" > "$T/check$b.out" <<'PY'
import sys, re
import xml.etree.ElementTree as ET
PSF = '{http://schemas.microsoft.com/windows/2003/08/printing/printschemaframework}'
raw = open(sys.argv[1], encoding='utf-8', errors='replace').read()
ok = []
def need(c, what): ok.append((bool(c), what))
try:
    root = ET.fromstring(raw[raw.find('<psf:'):].encode())
except Exception as e:
    print('BAD  the capabilities are not XML: %s (%r)' % (e, raw[:200])); sys.exit()
nsmap = dict(re.findall(r'xmlns:(\w+)="([^"]+)"', raw))
def feature(name):
    for f in root.findall(PSF + 'Feature'):
        if f.get('name') == name: return f
def options(f):
    return [] if f is None else f.findall(PSF + 'Option')
def scored(o, name):
    for s in o.findall(PSF + 'ScoredProperty'):
        if s.get('name') == name:
            v = s.find(PSF + 'Value')
            return None if v is None else int(v.text)
def display(o):
    for p in o.findall(PSF + 'Property'):
        if p.get('name') == 'psk:DisplayName': return p.find(PSF + 'Value').text
papers = {}
for line in open(sys.argv[2]):
    m = re.match(r'paper (\d+)\|(.*)\|(\d+)x(\d+)', line.strip())
    if m and int(m.group(3)): papers[m.group(2)] = (int(m.group(1)), int(m.group(3)) * 100, int(m.group(4)) * 100)
need(papers, 'DeviceCapabilities has papers')
media = options(feature('psk:PageMediaSize'))
by_name = {display(o): o for o in media}
for name, (pid, w, h) in papers.items():
    o = by_name.get(name)
    need(o is not None and scored(o, 'psk:MediaSizeWidth') == w and scored(o, 'psk:MediaSizeHeight') == h,
         'paper "%s" with its size %dx%d microns' % (name, w, h))
lab = by_name.get('30336 1 in x 2-1/8 in')
if lab is not None:
    pfx = lab.get('name').split(':')[0]
    need(pfx != 'psk' and nsmap.get(pfx), "the label is the driver's own option (%s), its namespace declared" % lab.get('name'))
    print('label-option', lab.get('name'))
need(by_name.get('Letter') is not None and by_name['Letter'].get('name') == 'psk:NorthAmericaLetter', 'Letter is psk:NorthAmericaLetter')
need(by_name.get('A4') is not None and by_name['A4'].get('name') == 'psk:ISOA4', 'A4 is psk:ISOA4')
need(any(o.get('name') == 'psk:CustomMediaSize' for o in media) and
     any(p.get('name') == 'psk:PageMediaSizeMediaSizeWidth' for p in root.findall(PSF + 'ParameterDef')),
     'a custom size with its range')
bins = {display(o): o.get('name') for o in options(feature('psk:JobInputBin'))}
need(all(n in bins for n in ('Automatically Select', 'Manual Feed', 'Tray 2')) and len(set(bins.values())) == len(bins),
     'the input bins by name: %s' % bins)
dup = [o.get('name') for o in options(feature('psk:JobDuplexAllDocumentsContiguously'))]
need(dup == ['psk:OneSided', 'psk:TwoSidedLongEdge', 'psk:TwoSidedShortEdge'], 'duplex: %s' % dup)
res = [(scored(o, 'psk:ResolutionX'), scored(o, 'psk:ResolutionY')) for o in options(feature('psk:PageResolution'))]
need((600, 600) in res, 'resolutions: %s' % res)
ori = [o.get('name') for o in options(feature('psk:PageOrientation'))]
need(ori == ['psk:Portrait', 'psk:Landscape'], 'orientations: %s' % ori)
col = [o.get('name') for o in options(feature('psk:PageOutputColor'))]
need('psk:Color' in col, 'colour: %s' % col)
copies = [p for p in root.findall(PSF + 'ParameterDef') if p.get('name') == 'psk:JobCopiesAllDocuments']
need(copies and any(q.get('name') == 'psf:MaxValue' and int(q.find(PSF + 'Value').text) > 1 for q in copies[0].findall(PSF + 'Property')),
     'copies with a maximum')
area = [p for p in root.findall(PSF + 'Property') if p.get('name') == 'psk:PageImageableSize']
need(area and len(area[0].findall('.//' + PSF + 'Property')) >= 6, 'the imageable area')
for c, w in ok: print(('ok   ' if c else 'BAD  ') + w)
PY
    if [ -s "$T/check$b.out" ] && ! grep -q '^BAD' "$T/check$b.out"; then
        pass "$b-bit: the capabilities have the driver's papers, sizes, bins, duplex, resolutions ($(grep -c '^ok' "$T/check$b.out") checks)"
    else
        fail "$b-bit: the capabilities:"; grep -v '^ok' "$T/check$b.out" | sed 's/^/      /'
    fi
    opt=$(sed -n 's/^label-option //p' "$T/check$b.out")
    lid=$(sed -n 's/^paper \([0-9]*\)|30336 1 in x 2-1\/8 in|.*/\1/p' "$T/dc$b.txt")
    [ -n "$opt" ] || opt="ns0000:Paper$lid"
    out=$(run "C:\\probe$b.exe" ticket $P "$opt" 25400 54300)
    printf '      %s -> %s\n' "$opt" "$out"
    case "$out" in "devmode paper $lid "*) pass "$b-bit: a ticket naming the label's option prints on the label (paper $lid)" ;;
        *) fail "$b-bit: a ticket naming $opt: '$out' (the label is paper $lid)" ;; esac
    out=$(run "C:\\probe$b.exe" ticket $P psk:CustomMediaSize 25400 54300)
    printf '      custom 25400x54300 -> %s\n' "$out"
    case "$out" in "devmode paper $lid "*) pass "$b-bit: a custom size the driver has a paper of is that paper" ;;
        *) fail "$b-bit: a custom 1 x 2-1/8 in size: '$out' (the label is paper $lid)" ;; esac
done
# the printer's status is its queue's: paused (cupsdisable) and back
CUPSDISABLE="${CUPSDISABLE:-/usr/sbin/cupsdisable}"; CUPSENABLE="${CUPSENABLE:-/usr/sbin/cupsenable}"
if [ -x "$CUPSDISABLE" ] && [ -x "$CUPSENABLE" ]; then
    "$CUPSDISABLE" $P; sleep 2.2
    for b in 64 32; do
        out=$(run "C:\\probe$b.exe" status $P)
        case "$out" in "status 0x1 0x1") pass "$b-bit: a paused queue is a paused printer (levels 2 and 6: $out)" ;;
            *) fail "$b-bit: a paused queue: '$out' (PRINTER_STATUS_PAUSED, 0x1, wanted)" ;; esac
    done
    "$CUPSENABLE" $P; sleep 2.2
    out=$(run 'C:\probe64.exe' status $P)
    case "$out" in "status 0x0 0x0") pass "a queue taking jobs again is ready ($out)" ;;
        *) fail "a queue taking jobs again: '$out'" ;; esac
else
    echo "      (no cupsdisable: the status is not checked)"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
