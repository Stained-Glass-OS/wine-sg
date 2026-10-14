#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# comdlg32 gap-A batch (patch 3220), 64- and 32-bit: test/fastcdlg-probe.c.
#   PrintDlgEx[AW] property sheet (printer list, page ranges, copies/collate, Apply, callback and
#   services, custom pages), the file dialog's IShellBrowser, CF_SELECTSCRIPT, CDM_SETDEFEXT,
#   shortcuts in Open dialogs, envelope sample pages of Page Setup.
# Printers are made on a FILE: port with CUPS unreachable: nothing is printed.
#
#   WINE=/opt/wine-sg/bin/wine test/fastcdlg-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_CDLG_* (comdlg32).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastcdlg.XXXXXX)
export CUPS_SERVER="$T/no-cups.sock" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in 64:x86_64 32:i686; do
    bits=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    TMPDIR=/var/tmp "$cc" -O1 -DUNICODE -D_UNICODE -o "$T/p$bits.exe" "$HERE/fastcdlg-probe.c" \
        -lcomdlg32 -lcomctl32 -lwinspool -lole32 -luuid -lshell32 -lgdi32 -luser32 \
        || { echo "FAIL  probe did not build ($bits)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for bits in 64 32; do
    cp "$T/p$bits.exe" "$WINEPREFIX/drive_c/"
    cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" p$bits.exe 2>/dev/null </dev/null | tr -d '\r' | grep -E '^(FAIL|DONE|RESULT)' > "$T/out$bits"
EOS
    chmod +x "$T/run.sh"
    timeout -s KILL 300 xvfb-run -a -s '-screen 0 1280x1024x24' "$T/run.sh"
    echo "== $bits"
    cat "$T/out$bits"
    grep -qx 'RESULT: PASS' "$T/out$bits" || RC=1
    "$WINESERVER" -k 2>/dev/null
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
