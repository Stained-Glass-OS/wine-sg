#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A CUPS printer's queue (patches/sg/9812, 9813). A Windows program's
# document waits in CUPS once it is spooled -- the program's own job ends
# when CUPS has the data -- so a printer's queue was always empty to Windows
# programs: nothing to see, pause or cancel when a printer jammed, cJobs 0,
# SetPrinter pause a flag nobody read, and a document named with accented or
# CJK characters reached CUPS cut short. The gate runs a CUPS server of its
# own with a stopped queue on file:///dev/null and, 64- and 32-bit, prints a
# document there and lists, pauses, resumes, restarts and cancels it
# (EnumJobs, GetJob, SetJob), counts it (cJobs); resumes, pauses and purges
# the printer (SetPrinter); and opens the queue window (printui /o), which
# lists the document and pauses it from its Document menu. And (9212) after
# the process pauses, resumes or purges a printer, or prints a document, the
# next GetPrinter asks CUPS again: status and cJobs follow at once.
# Mutant (winspool): SG_MUTANT_STATUS_STALE. And (9213) a user with printers
# but no default printer of their own (made by the system or another account)
# is given the CUPS default (mutant SG_MUTANT_NO_DEFAULT_CHOICE).
#
#   WINE=/opt/wine-sg/bin/wine test/cupsjobs-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
LPADMIN="${LPADMIN:-/usr/sbin/lpadmin}"
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$LPADMIN" ] || { echo "SKIP: no lpadmin"; exit 77; }
command -v cupsdisable >/dev/null || { echo "SKIP: no cupsdisable"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-cupsjobs.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
cleanup() {
    "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"
    [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

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

for m in "$MINGW:64" "$MINGW32:32"; do
    "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/cupsjobs-probe.c" -lwinspool -lcomctl32 -lshell32 -lcomdlg32 ||
        { echo "FAIL  probe did not build"; exit 1; }
done
queue() {
    "$LPADMIN" -p "$1" -E -v file:///dev/null -m raw 2>/dev/null || "$LPADMIN" -p "$1" -E -v file:///dev/null 2>/dev/null
    cupsdisable "$1" 2>/dev/null
    lpstat -p "$1" 2>/dev/null | grep -q disabled || { echo "SKIP: the gate's CUPS server did not take queue $1"; exit 77; }
}
# the queues, made before Wine first lists the printers; stopped so documents wait
for q in SG_Jobs_64 SG_Printer_64 SG_Jobs_32 SG_Printer_32 SG_Window SG_Status_64 SG_Status_32; do queue "$q"; done
"$LPADMIN" -d SG_Window 2>/dev/null
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
run() {
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -q '^RESULT: PASS'
}
for b in 64 32; do
    if run "C:\\probe$b.exe" jobs "SG_Jobs_$b"; then pass "$b-bit: a document waiting in CUPS is the printer's job"
    else fail "$b-bit: the CUPS queue's documents are not the printer's jobs"; fi
    if run "C:\\probe$b.exe" printer "SG_Printer_$b"; then pass "$b-bit: the printer pauses, resumes and purges in CUPS"
    else fail "$b-bit: pausing, resuming or purging the printer does not reach CUPS"; fi
    if run "C:\\probe$b.exe" status "SG_Status_$b"; then pass "$b-bit: GetPrinter shows the process's own pause, document and purge at once (9212)"
    else fail "$b-bit: GetPrinter shows the process's own changes late (CUPS state kept 2 s)"; fi
    if run "C:\\probe$b.exe" default SG_Window; then pass "$b-bit: a user with no default printer is given the CUPS default; the Print dialog has it (9213)"
    else fail "$b-bit: a user with no default printer has none (the classic Print dialog lists no printer)"; fi
done
# the window needs a display: the server (and its desktop) started without one goes first
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
cat > "$T/window.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c" && WINEDEBUG=err+all,+printui timeout -s KILL 120 "$WINE" 'C:\\probe64.exe' window SG_Window 2>"$T/window.err" </dev/null | tr -d '\r' > "$T/window.out"
EOS
chmod +x "$T/window.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x768x24' "$T/window.sh"
sed 's/^/      /' "$T/window.out"
if grep -q '^RESULT: PASS' "$T/window.out"; then pass "the queue window lists the printer's documents and controls them"
else fail "the queue window does not list or control the printer's documents"; tail -20 "$T/window.err"; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
