#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Real document work in LibreOffice for Windows under wine-sg: not "the
# window opens" (the compat suite covers that) but open, edit, save and print
# Office Open XML files, as a person uses an office suite.
#
#  1. a .docx (Calibri text, a table) exports to PDF with its text, and the
#     PDF embeds the metric-compatible font the suite substitutes (Carlito);
#  2. a .xlsx whose formulas have no cached values is recalculated on load
#     and exported to CSV (3.75 / 10 / 13.75);
#  3. edited through UNO from LibreOffice's own Python (a named-pipe bridge
#     between two processes): B2 := 10, recalculated, saved as .xlsx -- the
#     saved file reads back 22.5;
#  4. a .pptx exports to PDF with its slide text;
#  5. .docx -> .odt -> .docx round trip keeps the text;
#  6. printing: `soffice --pt` to a printer (a private cupsd) produces a job.
#
# The installer comes from the compat suite's cache (apps.list's LibreOffice
# line, hash-checked; downloaded unless OFFLINE=1). LibreOffice's own online
# updater is switched off by machine policy first, as an administrator would:
# the pinned build otherwise updates itself release by release on first start.
#
#   WINE=/opt/wine-sg/bin/wine test/office-docs-gate.sh
#   ARTIFACTS=DIR keeps the outputs and logs
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
CACHE="${CACHE:-$SG_REAL_HOME/.cache/sg-compat}"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in xvfb-run python3 pdftotext pdffonts ps2ascii sha256sum curl; do
    command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }
done
[ -x /usr/sbin/cupsd ] && [ -x /usr/sbin/lpadmin ] || { echo "SKIP: needs cups-daemon and cups-client"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

line=$(grep '^LibreOffice ' "$HERE/compat/apps.list" | head -1)
file=$(echo "$line" | cut -d'|' -f2); sha=$(echo "$line" | cut -d'|' -f3); url=$(echo "$line" | cut -d'|' -f4)
iargs=$(echo "$line" | cut -d'|' -f6)
mkdir -p "$CACHE"
if [ ! -s "$CACHE/$file" ] && [ "${OFFLINE:-0}" != 1 ]; then
    curl -sL --max-time 1800 -o "$CACHE/$file.part" "$url" && mv "$CACHE/$file.part" "$CACHE/$file"
fi
[ -s "$CACHE/$file" ] && [ "$(sha256sum "$CACHE/$file" | cut -d' ' -f1)" = "$sha" ] ||
    { echo "SKIP: no LibreOffice installer ($file)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-office-docs.XXXXXX); chmod 755 "$T"
CP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$CP" ] && kill "$CP" 2>/dev/null
    if [ -n "${ARTIFACTS:-}" ]; then
        mkdir -p "$ARTIFACTS"; cp -r "$T/out" "$ARTIFACTS/" 2>/dev/null; cp "$T"/*.log "$ARTIFACTS/" 2>/dev/null
    fi
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
# A scratch HOME and no menu/desktop integration: a prefix links its Desktop,
# Documents, Downloads... to $HOME's, and installers write shortcuts there.
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop"
mkdir -p "$HOME/Desktop"
export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"

# a private CUPS with one raw queue (as notepad-print-gate): Wine lists its
# printers from CUPS. Debian's cupsd is AppArmor-confined and cannot write a
# file:// device outside its own directories, so the job is captured on the
# Wine side instead: HKCU\Software\Wine\Printing\Spooler maps the queue's
# port (CUPS:sgdocs) to a Unix file -- what LibreOffice printed through GDI
# and the PostScript driver, byte for byte.
mkdir -p "$T/cups/spool" "$T/cups/cache" "$T/cups/state" "$T/cups/logs" "$T/cups/tmp"
cat > "$T/cups/cupsd.conf" <<EOF
Listen $T/cups/cups.sock
LogLevel warn
DefaultAuthType None
WebInterface No
<Location />
  Order allow,deny
  Allow all
</Location>
<Location /admin>
  Order allow,deny
  Allow all
</Location>
EOF
cat > "$T/cups/cups-files.conf" <<EOF
ServerRoot $T/cups
CacheDir $T/cups/cache
StateDir $T/cups/state
RequestRoot $T/cups/spool
TempDir $T/cups/tmp
ServerBin /usr/lib/cups
DataDir /usr/share/cups
AccessLog $T/cups/logs/access_log
ErrorLog $T/cups/logs/error_log
PageLog $T/cups/logs/page_log
FileDevice Yes
Sandboxing relaxed
EOF
/usr/sbin/cupsd -f -c "$T/cups/cupsd.conf" -s "$T/cups/cups-files.conf" >/dev/null 2>&1 & CP=$!
export CUPS_SERVER="$T/cups/cups.sock"
i=0; while [ ! -S "$CUPS_SERVER" ] && [ $i -lt 20 ]; do sleep 0.3; i=$((i + 1)); done
/usr/sbin/lpadmin -p sgdocs -E -v "file:///dev/null" && /usr/sbin/lpadmin -d sgdocs ||
    { echo "FAIL  no test printer"; exit 1; }

python3 "$HERE/office-docs/mkdocs.py" "$T/docs" || { echo "FAIL  fixtures"; exit 1; }
cp "$HERE/office-docs/edit.py" "$T/docs/"
mkdir -p "$T/out"

export WINEPREFIX="$T/prefix" WINEDEBUG=-all
"$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
K='HKLM\Software\Policies\LibreOffice\org.openoffice.Office.Update\Update\Enabled'
"$WINE" reg add "$K" /v Value /d false /f >/dev/null 2>&1
"$WINE" reg add "$K" /v Final /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v CUPS:sgdocs /d "$T/out.prn" /f >/dev/null 2>&1
"$WINESERVER" -w
cp -r "$T/docs/." "$WINEPREFIX/drive_c/"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.*) echo "refusing DISPLAY :0"; exit 1 ;; esac
cd "$WINEPREFIX/drive_c"
WINEDEBUG=trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
timeout -s KILL 1500 "$WINE" msiexec /i "\$("$WINE" winepath -w "$CACHE/$file" | tr -d '\r')" /qn $iargs > "$T/install.log" 2>&1
echo "install_rc=\$?" > "$T/r"
SO='C:\\Program Files\\LibreOffice\\program\\soffice.com'
lo() { timeout -s KILL 300 "$WINE" "\$SO" --headless --norestore "\$@" >> "$T/soffice.log" 2>&1; }
# first start: the user profile is created and the process restarts itself
lo --terminate_after_init
lo --convert-to pdf --outdir 'C:\\out\\docx' 'C:\\src.docx'
lo --convert-to csv --outdir 'C:\\out\\xlsx' 'C:\\src.xlsx'
timeout -s KILL 300 "$WINE" 'C:\\Program Files\\LibreOffice\\program\\python.exe' 'C:\\edit.py' 'C:\\src.xlsx' 'C:\\out\\edited.xlsx' > "$T/uno.log" 2>&1
lo --convert-to csv --outdir 'C:\\out\\edited' 'C:\\out\\edited.xlsx'
lo --convert-to pdf --outdir 'C:\\out\\pptx' 'C:\\src.pptx'
lo --convert-to odt --outdir 'C:\\out\\odt' 'C:\\src.docx'
lo --convert-to 'docx:MS Word 2007 XML' --outdir 'C:\\out\\rt' 'C:\\out\\odt\\src.odt'
lo --convert-to pdf --outdir 'C:\\out\\rtpdf' 'C:\\out\\rt\\src.docx'
lo --pt sgdocs 'C:\\src.docx'
EOF
chmod +x "$T/session.sh"
nice timeout -s KILL 3600 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh" > "$T/session.log" 2>&1
O="$WINEPREFIX/drive_c/out"
cp -r "$O/." "$T/out/" 2>/dev/null

grep -q '^install_rc=0' "$T/r" 2>/dev/null && pass "LibreOffice installs (msiexec /qn)" || fail "install: $(cat "$T/r" 2>/dev/null)"

txt=$(pdftotext "$O/docx/src.pdf" - 2>/dev/null | tr '\n' ' ')
case "$txt" in *Zebra4217*alpha*delta*) pass ".docx -> PDF keeps its text and table" ;; *) fail ".docx -> PDF: '$txt'" ;; esac
fonts=$(pdffonts "$O/docx/src.pdf" 2>/dev/null | tail -n +3 | awk '{print $1}' | tr '\n' ' ')
case "$fonts" in *Carlito*) pass "Calibri text is set in the metric-compatible Carlito ($fonts)" ;; *) fail "fonts in the PDF: '$fonts'" ;; esac

csv=$(tr -d '\r' < "$O/xlsx/src.csv" 2>/dev/null | tr '\n' ' ')
case "$csv" in *"apple,3,1.25,3.75"*"sum,,,13.75"*) pass ".xlsx formulas are calculated on load ($csv)" ;; *) fail ".xlsx -> CSV: '$csv'" ;; esac

uno=$(tr -d '\r' < "$T/uno.log" | grep -E '^(before|after|saved)' | tr '\n' ' ')
case "$uno" in *"after 22.5"*saved*) pass "UNO over a named pipe edits, recalculates and saves ($uno)" ;; *) fail "UNO edit: '$uno' $(tail -3 "$T/uno.log")" ;; esac
csv2=$(tr -d '\r' < "$O/edited/edited.csv" 2>/dev/null | tr '\n' ' ')
case "$csv2" in *"apple,10,1.25,12.5"*"sum,,,22.5"*) pass "the saved .xlsx reads back edited ($csv2)" ;; *) fail "edited .xlsx: '$csv2'" ;; esac

ptxt=$(pdftotext "$O/pptx/src.pdf" - 2>/dev/null | tr '\n' ' ')
case "$ptxt" in *Quokka9031*) pass ".pptx -> PDF keeps the slide's text" ;; *) fail ".pptx -> PDF: '$ptxt'" ;; esac

rtxt=$(pdftotext "$O/rtpdf/src.pdf" - 2>/dev/null | tr '\n' ' ')
[ -s "$O/odt/src.odt" ] && case "$rtxt" in *Zebra4217*gamma*) true ;; *) false ;; esac &&
    pass ".docx -> .odt -> .docx keeps the text" || fail "round trip: odt=$(ls "$O/odt" 2>/dev/null) text='$rtxt'"

# printing: text reaches the page as Direct2D-drawn bitmaps (0300/0301) --
# test/dcrt-print-gate.sh checks that path; here only that a job was made
[ -s "$T/out.prn" ] && pass "printing produces a PostScript job ($(wc -c < "$T/out.prn") bytes)" || fail "nothing was printed"

[ $RC = 0 ] && echo "office-docs-gate: all passed" || echo "office-docs-gate: FAILURES"
exit $RC
