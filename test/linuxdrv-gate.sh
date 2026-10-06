#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program prints through a printer's Windows driver (patches/sg/0922,
# with sg-session's sgwindrv backend).
#
# A printer with a maker's Windows driver gets a CUPS queue for Linux
# programs whose PPD is made from the driver's capabilities (splwow64 ppd)
# and whose backend, sgwindrv, draws the job's PDF pages and prints them
# through the driver (splwow64 print); the driver's output goes to the
# printer's own CUPS queue as a raw job. The gate runs a CUPS server of its
# own (no root, no printer) with the backend from sg-session (SGWINDRV, by
# default the installed one), and our test driver ("SG Test UMPD", 2 x 1 in
# at 100 dpi, printable from 5,5) for the printer "SGLabel". A PDF of a 2 x 1
# inch page with a black box at 0.2,0.1-1.2,0.6 inch must come out of the
# driver as that box on its page.
#
#   WINE=/opt/wine-sg/bin/wine SGWINDRV=.../sg-session/bin/sgwindrv test/linuxdrv-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
SGWINDRV="${SGWINDRV:-/usr/lib/cups/backend/sgwindrv}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v pdftoppm >/dev/null || { echo "SKIP: no pdftoppm"; exit 77; }
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -r "$SGWINDRV" ] || { echo "SKIP: no sgwindrv backend at $SGWINDRV"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-linuxdrv.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock"
stop_cups() { [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; CP=; }; }
trap '"$WINESERVER" -k 2>/dev/null; stop_cups; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

# the backend runs the Windows side through these: as "SYSTEM" (here, us)
# with the machine's prefix (here, the gate's)
mkdir -p "$T/lib" "$T/bin" "$T/serverbin/backend"
cat > "$T/lib/sg-common.sh" <<EOF
sg_wine_env() { WINEPREFIX="$WINEPREFIX"; WINESERVER="$WINESERVER"; PATH="$T/bin:\$PATH"; export WINEPREFIX WINESERVER PATH; }
EOF
cat > "$T/bin/runuser" <<'EOF'
#!/bin/sh
# runuser -u USER -- CMD...: the gate is that user
while [ $# -gt 0 ] && [ "$1" != "--" ]; do shift; done
shift
exec "$@"
EOF
printf '#!/bin/sh\nexec "%s" "$@"\n' "$WINE" > "$T/bin/wine"
chmod +x "$T/bin/runuser" "$T/bin/wine"
cp "$SGWINDRV" "$T/serverbin/backend/sgwindrv"; chmod 0755 "$T/serverbin/backend/sgwindrv"
for d in filter daemon cgi-bin monitor notifier; do [ -e "/usr/lib/cups/$d" ] && ln -s "/usr/lib/cups/$d" "$T/serverbin/$d"; done

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
ServerBin $T/serverbin
DataDir /usr/share/cups
SetEnv SG_LIB $T/lib
SetEnv SG_RUNUSER $T/bin/runuser
User $(id -un)
Group $(id -gn)
EOF
cat > "$T/root/printers.conf" <<EOF
<Printer SGLabel>
Info Label printer
MakeModel SG Test UMPD
DeviceURI file:///dev/null
State Idle
Accepting Yes
</Printer>
EOF
sed -n '/^\*PPD-Adobe/,/^\*Font Courier/p' "$HERE/cupsdrv-gate.sh" | sed 's/\$1/SG Test UMPD/g' > "$T/root/ppd/SGLabel.ppd"
# a copy of cupsd: the system's is confined (AppArmor) to its own backends
cp "$CUPSD" "$T/cupsd"
start_cups() {
    "$T/cupsd" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
    i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
}
start_cups
lpstat -v 2>/dev/null | grep -q SGLabel || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

printf 'LIBRARY gdi32.dll\nEXPORTS\nEngCreateBitmap\nEngAssociateSurface\nEngDeleteSurface\nEngWritePrinter\n' > "$T/eng.def"
"${MINGW%-gcc}-dlltool" -d "$T/eng.def" -l "$T/libeng.a" || { echo "SKIP: no dlltool"; exit 77; }
"$MINGW" -shared -O2 -o "$T/sgtestumpd.dll" "$HERE/printdrv-umpd.c" "$T/libeng.a" -lgdi32 -lwinspool || { echo "FAIL  driver did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/"
S="$C/windows/system32/DriverStore/FileRepository/sgtestumpd.inf_1"
mkdir -p "$S/amd64"
cp "$T/sgtestumpd.dll" "$S/amd64/"
printf 'SG test data\r\n' > "$S/sgtest.dat"
sed -n '/^\[Version\]/,/^sgtest.dat = 2/p' "$HERE/cupsdrv-gate.sh" > "$S/sgtestumpd.inf"
run() { (cd "$C" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }

out=$(run splwow64.exe list)
[ "$out" = "$(printf 'SGLabel\tSG Test UMPD')" ] && pass "the printers with a Windows driver are listed for CUPS" ||
    fail "splwow64 list: '$out'"

run splwow64.exe ppd SGLabel "Z:$(printf '%s' "$T/win.ppd" | tr / '\\')" >/dev/null
if grep -q '^\*PageSize P256/SG Test 2x1: "<</PageSize\[151.09 79.09\]' "$T/win.ppd" 2>/dev/null &&
   grep -q '^\*ImageableArea P256: "3.60 3.60 147.60 75.60"' "$T/win.ppd" &&
   grep -q "^\\*NickName: \"SGLabel (maker's driver)\"" "$T/win.ppd" &&
   grep -q '^\*cupsFilter2: "application/pdf application/vnd.cups-pdf 0 -"' "$T/win.ppd"; then
    pass "the queue's PPD is made from the driver's capabilities (paper, printable area)"
else
    fail "the PPD made from the driver:"; sed -n '/PageSize P\|ImageableArea P\|NickName\|cupsFilter/p' "$T/win.ppd" 2>/dev/null | sed 's/^/      /'
fi

# the queue for Linux programs, as sg-windows-printers makes it
stop_cups
cat >> "$T/root/printers.conf" <<EOF
<Printer SGLabel_Windows>
Info SGLabel (maker's driver)
MakeModel SGLabel (maker's driver)
DeviceURI sgwindrv:/SGLabel
State Idle
Accepting Yes
</Printer>
EOF
cp "$T/win.ppd" "$T/root/ppd/SGLabel_Windows.ppd"
start_cups

# a Linux program's PDF: one page W x H points with a black rectangle
mkpdf() {
    python3 - "$@" <<'EOF'
import sys
out_file, w, h, rect = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
content = b"0 g " + rect.encode() + b" re f\n"
objs = [b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %s %s] /Contents 4 0 R >>" % (w.encode(), h.encode()),
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"endstream"]
out = b"%PDF-1.4\n"; offs = []
for i, o in enumerate(objs):
    offs.append(len(out)); out += b"%d 0 obj\n" % (i + 1) + o + b"\nendobj\n"
x = len(out)
out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1) + b"".join(b"%010d 00000 n \n" % o for o in offs)
out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, x)
open(out_file, "wb").write(out)
EOF
}
# prints a PDF on the queue and leaves the driver's page report in $page
lpjob() {
    _done=$(grep -c 'Job completed' "$T/log/error_log" 2>/dev/null)
    rm -f "$T/spool"/d*-001
    lp -d SGLabel_Windows "$@" >/dev/null 2>&1 || fail "lp refused the job"
    i=0; while [ $i -lt 90 ]; do
        [ "$(grep -c 'Job completed' "$T/log/error_log" 2>/dev/null)" -ge $((_done + 2)) ] && break
        sleep 1; i=$((i + 1))
    done
    raw=""
    for f in "$T/spool"/d*-001; do head -1 "$f" 2>/dev/null | grep -q '^SGTD START' && raw=$f; done
    page=$(grep -a '^PAGE' "$raw" 2>/dev/null | tr -d '\r')
    dark=$(printf '%s' "$page" | sed -n 's/.*dark=\([0-9]*\).*/\1/p')
    box=$(printf '%s' "$page" | sed -n 's/.* box=\(.*\)/\1/p')
}

# a 2 x 1 inch page, a black box 0.2,0.1-1.2,0.6 inch, on the PPD's paper
mkpdf "$T/job.pdf" 151.09 79.09 "14.4 35.89 72 36"
lpjob -o PageSize=P256 "$T/job.pdf"
if [ -n "$raw" ] && [ -n "$dark" ] && [ "$dark" -ge 4700 ] && [ "$dark" -le 5300 ] &&
   case "$page" in "PAGE 1 200x100 "*) true ;; *) false ;; esac &&
   case "$box" in 1[4-6],[4-6]-11[3-5],5[3-5]) true ;; *) false ;; esac; then
    pass "the Linux program's page came out of the Windows driver: $page"
else
    fail "the Windows driver's output for the Linux job: '${page:-none}'"
    grep -a 'Job [0-9]' "$T/log/error_log" 2>/dev/null | grep -v 'argv\|envp' | tail -12 | sed 's/^/      /'
fi

# the label laid out across (1 x 2 inch, a box 0.1,0.2-0.6,1.2 inch), the
# paper named by size alone (Firefox's PageSize=Custom.WxH): the driver's
# paper of that size, turned
mkpdf "$T/across.pdf" 79.09 151.09 "7.2 64.69 36 72"
lpjob -o PageSize=Custom.79.09x151.09 "$T/across.pdf"
if [ -n "$raw" ] && [ -n "$dark" ] && [ "$dark" -ge 4700 ] && [ "$dark" -le 5300 ] &&
   case "$page" in "PAGE 1 100x200 "*) true ;; *) false ;; esac &&
   case "$box" in [4-6],1[4-6]-5[3-5],11[3-5]) true ;; *) false ;; esac; then
    pass "a page laid out across prints on the driver's paper of its size, turned: $page"
else
    fail "the page laid out across: '${page:-none}'"
fi

[ $RC = 0 ] && echo "linuxdrv gate: PASS" || echo "linuxdrv gate: FAIL"
exit $RC
