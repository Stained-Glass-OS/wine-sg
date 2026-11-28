#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WordPad's /p (patches/sg/9761): Windows' WordPad prints the document on the default printer and
# closes; it is the print verb of .rtf and .wri files (wordpad.exe /p "%1": File Explorer's Print).
# Wine's said "Printing not implemented." and printed nothing. The gate makes a PostScript printer
# whose port is a file the default, prints an RTF with wordpad.exe /p under a private Xvfb, and
# reads the job's text back (ghostscript pdfwrite, pdftotext).
#   WINE=/opt/wine-sg/bin/wine test/wordpadprint-gate.sh
# Mutant: SG_MUTANT_WORDPAD_NO_PRINT (programs/wordpad/wordpad.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb gs pdftotext "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wordpadprint.XXXXXX)
DPY=213; while [ -e "/tmp/.X$DPY-lock" ] || [ -e "/tmp/.X11-unix/X$DPY" ]; do DPY=$((DPY + 1)); done
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
cat > "$T/addprn.c" <<'EOF'
#include <windows.h>
#include <winspool.h>
#include <stdio.h>
/* addprn PORT: a PostScript printer whose port is the file PORT, the default printer */
int wmain(int argc, WCHAR **argv)
{
    static const char ppd[] =
        "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
        "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGWP.PPD\"\n*Manufacturer: \"SG\"\n*ModelName: \"SG WP\"\n"
        "*NickName: \"SG WP\"\n*ShortNickName: \"SG WP\"\n*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n"
        "*TTRasterizer: Type42\n*OpenUI *PageSize/Page Size: PickOne\n*DefaultPageSize: Letter\n"
        "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
        "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"18 18 594 774\"\n"
        "*DefaultPaperDimension: Letter\n*PaperDimension Letter/Letter: \"612 792\"\n*DefaultResolution: 300dpi\n";
    DRIVER_INFO_3W d = {0};
    PRINTER_INFO_2W pi = {0};
    HANDLE h;
    FILE *f;
    if (argc < 2) return 2;
    f = _wfopen(L"C:\\sgwp.ppd", L"w"); fputs(ppd, f); fclose(f);
    d.cVersion = 3; d.pName = (WCHAR *)L"SG WP PS"; d.pDriverPath = d.pConfigFile = (WCHAR *)L"wineps.drv";
    d.pDataFile = (WCHAR *)L"C:\\sgwp.ppd"; d.pDefaultDataType = (WCHAR *)L"RAW";
    if (!AddPrinterDriverExW(NULL, 3, (BYTE *)&d, APD_COPY_NEW_FILES | APD_COPY_FROM_DIRECTORY)) { puts("nodriver"); return 1; }
    pi.pPrinterName = (WCHAR *)L"SG WordPad Printer"; pi.pDriverName = d.pName; pi.pPortName = argv[1];
    pi.pPrintProcessor = (WCHAR *)L"wineps"; pi.pDatatype = (WCHAR *)L"RAW";
    if (!(h = AddPrinterW(NULL, 2, (BYTE *)&pi))) { printf("noprinter %lu\n", GetLastError()); return 1; }
    ClosePrinter(h);
    if (!SetDefaultPrinterW(L"SG WordPad Printer")) { puts("nodefault"); return 1; }
    puts("ok");
    return 0;
}
EOF
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/addprn.exe" "$T/addprn.c" -lwinspool || { fail "the printer adder did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export CUPS_SERVER="$T/no-cups.sock"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
PS="$T/job.ps"
cp "$T/addprn.exe" "$C/"
added=$(cd "$C" && timeout -s KILL 120 "$WINE" 'C:\addprn.exe' "$PS" 2>/dev/null | tr -d '\r')
[ "$added" = ok ] || { fail "the default printer could not be set up ($added)"; exit 1; }
printf '{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 DejaVu Sans;}}\\f0\\fs28 WordPad prints this line.\\par Second line of the document.\\par}\n' > "$C/doc.rtf"
start=$(date +%s)
(cd "$C" && timeout -s KILL 120 "$WINE" 'C:\Program Files\Windows NT\Accessories\wordpad.exe' /p 'C:\doc.rtf' >/dev/null 2>&1); wrc=$?
took=$(( $(date +%s) - start ))
"$WINESERVER" -w
[ $wrc = 0 ] && [ $took -lt 100 ] && pass "wordpad.exe /p ends by itself (${took}s)" || fail "wordpad.exe /p did not end by itself (rc $wrc, ${took}s)"
[ -s "$PS" ] && pass "a job was printed on the default printer" || fail "nothing was printed"
if [ -s "$PS" ]; then
    gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pdfwrite -sOutputFile="$T/job.pdf" "$PS" >/dev/null 2>&1
    txt=$(pdftotext "$T/job.pdf" - 2>/dev/null | tr -s ' \n' '  ')
    case "$txt" in *"WordPad prints this line."*"Second line of the document."*) pass "the page has the document's text" ;;
        *) fail "the page does not have the document's text: '$(printf '%.80s' "$txt")'" ;; esac
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
