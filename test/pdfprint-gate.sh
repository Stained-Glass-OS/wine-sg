#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Print to PDF printer and Print to file (patches/sg/9814). Windows asks
# where a printed file goes with the Save dialog "Save Print Output As":
# Printer Files (*.prn) for Print to file, a PDF named after the document for
# Microsoft Print to PDF. Our Print to PDF printer is a CUPS queue on cups-pdf,
# which put every PDF in Documents without asking, and Print to file asked in
# a bare "Output File Name" box. The gate runs a CUPS server of its own with
# a queue on cups-pdf (a stand-in backend: the job must never reach it) and a
# PostScript printer, and prints from a Windows program: the PDF printer asks,
# names the file after the document and saves a PDF there (Ghostscript); the
# printer's port as output asks too; Cancel prints nothing (ERROR_CANCELLED);
# a file the program names is not asked for and is a PDF; Print to file asks
# with Printer Files and saves the printer's PostScript; a printer prints
# without asking.
#
#   WINE=/opt/wine-sg/bin/wine test/pdfprint-gate.sh
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
command -v gs >/dev/null || { echo "SKIP: no Ghostscript (gs)"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -f /usr/share/ppd/cups-pdf/CUPS-PDF_opt.ppd ] || { echo "SKIP: no cups-pdf PPD (printer-driver-cups-pdf)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-pdfprint.XXXXXX); CP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
cleanup() {
    "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"
    [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
# CUPS's programs, but a cups-pdf backend of the gate's own (the real one is root's; no job may reach it)
mkdir -p "$T/serverbin/backend"
for d in filter cgi-bin daemon driver monitor notifier; do ln -s "/usr/lib/cups/$d" "$T/serverbin/$d"; done
ln -s /usr/lib/cups/backend/file "$T/serverbin/backend/file" 2>/dev/null
cat > "$T/serverbin/backend/cups-pdf" <<'EOB'
#!/bin/sh
[ $# = 0 ] && { echo 'file cups-pdf:/ "Virtual PDF Printer" "PDF Printer"'; exit 0; }
cat > /dev/null; exit 0
EOB
chmod 755 "$T/serverbin/backend/cups-pdf"
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
ServerBin $T/serverbin
DataDir /usr/share/cups
FileDevice Yes
User $(id -un)
Group $(id -gn)
EOF
: > "$T/root/printers.conf"
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
[ -S "$T/cups.sock" ] || { echo "SKIP: the gate's CUPS server did not start"; cat "$T/cupsd.out"; exit 77; }

"$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/pdfprint-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
"$LPADMIN" -p SG_Print_to_PDF -E -v cups-pdf:/ -P /usr/share/ppd/cups-pdf/CUPS-PDF_opt.ppd -D "Print to PDF" 2>/dev/null
"$LPADMIN" -p SG_Laser -E -v file:///dev/null -P /usr/share/ppd/cups-pdf/CUPS-PDF_opt.ppd -D "Laser" 2>/dev/null
lpstat -v SG_Print_to_PDF 2>/dev/null | grep -q cups-pdf || { echo "SKIP: the gate's CUPS server did not take the PDF queue"; exit 77; }
lpstat -v SG_Laser >/dev/null 2>&1 || { echo "SKIP: the gate's CUPS server did not take the PostScript queue"; exit 77; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/out"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c" && timeout -s KILL 240 "$WINE" 'C:\\probe.exe' SG_Print_to_PDF SG_Laser 'C:\\out' 2>"$T/probe.err" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
sed 's/^/      /' "$T/probe.out"
if grep -q '^RESULT: PASS' "$T/probe.out"; then echo "PASS  the PDF printer and Print to file ask Save Print Output As and save what they should"; echo "RESULT: PASS"; exit 0; fi
echo "FAIL  the PDF printer or Print to file"; tail -5 "$T/probe.err"; echo "RESULT: FAIL"; exit 1
