#!/bin/sh
# The printer-driver corpus: installs a printer maker's driver package in a
# scratch prefix and prints our test page through it, 64-bit and 32-bit, to
# a file; then checks the output (check.py). Vendor packages are not ours to
# redistribute: fetch them from the makers or the Microsoft Update Catalog
# and unpack them (cabextract) into a directory; see docs/printer-drivers.md.
#
#   tools/printer-corpus/run.sh PACKAGE_DIR [MODEL] [OUTDIR]
#
# WINE (default /opt/wine-sg/bin/wine) and WINESERVER choose the Wine.
# The prefix and HOME are scratch (never the user's), and are removed.
# Prints a summary line:
#   CORPUS <package> | <model> | install=<hr> | print64=<result> | print32=<result>
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/../../test/scratch-home.sh"
PKG=$(cd "$1" && pwd)
MODEL=${2:-}
NAME=$(basename "$PKG")
OUT=${3:-/var/tmp/sg-corpus-out/$NAME}
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$OUT"
T=$(mktemp -d /var/tmp/sg-corpus.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${WINEDEBUG:--all}" WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export CUPS_SERVER="$T/no-cups.sock"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/sgprint64.exe" "$HERE/sgprint.c" -lwinspool -lgdi32 -lsetupapi || exit 1
"$MINGW32" -municode -O2 -o "$T/sgprint32.exe" "$HERE/sgprint.c" -lwinspool -lgdi32 -lsetupapi || exit 1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/sgprint64.exe" "$T/sgprint32.exe" "$C/"
# the package's printer INF: the one naming a data file, for 64-bit Windows
inf_text() { if file "$1" | grep -q UTF-16; then iconv -f utf-16 -t utf-8 "$1"; else cat "$1"; fi | tr -d '\r'; }
INF=${INF:-}
if [ -z "$INF" ]; then
    for f in $(cd "$PKG" && ls *.inf *.INF 2>/dev/null); do
        t=$(inf_text "$PKG/$f")
        echo "$t" | grep -qi '^ *DataFile' || continue
        echo "$t" | grep -i -A2 '^\[Manufacturer' | grep -qi 'NTamd64' || continue
        INF=$f; break
    done
fi
[ -n "$INF" ] || INF=$(cd "$PKG" && ls *.inf *.INF 2>/dev/null | head -1)
[ -n "$INF" ] || { echo "CORPUS $NAME | no INF"; exit 1; }
S="$C/windows/system32/DriverStore/FileRepository/$(echo "$INF" | tr 'A-Z' 'a-z')_sg"
mkdir -p "$S"
cp -r "$PKG"/. "$S/"
rm -f "$S/pkg.cab"
run() { (cd "$C" && timeout 300 "$WINE" "$@" 2>>"$OUT/wine.log" | tr -d '\r'); }
WINF="C:\\windows\\system32\\DriverStore\\FileRepository\\$(basename "$S")\\$INF"
if [ -z "$MODEL" ]; then
    run 'C:\sgprint64.exe' models "$WINF" > "$OUT/models.txt"
    MODEL=$(sed -n 's/^model //p' "$OUT/models.txt" | head -1)
fi
inst=$(run 'C:\sgprint64.exe' install "$MODEL" | tail -1)
echo "$inst" > "$OUT/install.txt"
"$WINESERVER" -w
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT1:" /d "$OUT/out64.prn" /f >/dev/null 2>&1
timeout 60 "$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v "LPT2:" /d "$OUT/out32.prn" /f >/dev/null 2>&1
run 'C:\sgprint64.exe' add "Corpus 64" "$MODEL" "LPT1:" > "$OUT/add.txt"
run 'C:\sgprint64.exe' add "Corpus 32" "$MODEL" "LPT2:" >> "$OUT/add.txt"
run 'C:\sgprint64.exe' info "Corpus 64" > "$OUT/info.txt"
run 'C:\sgprint64.exe' caps "Corpus 64" > "$OUT/caps.txt"
run 'C:\sgprint64.exe' papers "Corpus 64" > "$OUT/papers.txt"
run 'C:\sgprint64.exe' features "Corpus 64" > "$OUT/features.txt"
rm -f "$OUT/out64.prn" "$OUT/out32.prn"
run 'C:\sgprint64.exe' print "Corpus 64" > "$OUT/print64.txt"
run 'C:\sgprint32.exe' print "Corpus 32" > "$OUT/print32.txt"
"$WINESERVER" -w
# the reference page: the sheet the driver reported
sheet=$(sed -n 's/.*sheet \([0-9]*\)x\([0-9]*\) mils.*/\1 \2/p' "$OUT/print64.txt")
set -- $sheet
if [ $# = 2 ]; then
    run 'C:\sgprint64.exe' ref 'C:\ref.bmp' $(( $1 * 254 / 1000 )) $(( $2 * 254 / 1000 )) 100 >/dev/null
    cp "$C/ref.bmp" "$OUT/ref.bmp" 2>/dev/null
fi
r64=$(python3 "$HERE/check.py" "$OUT/out64.prn" "$OUT/ref.bmp" | tee "$OUT/check64.txt" | sed -n 's/^RESULT //p')
r32=$(python3 "$HERE/check.py" "$OUT/out32.prn" "$OUT/ref.bmp" | tee "$OUT/check32.txt" | sed -n 's/^RESULT //p')
echo "CORPUS $NAME | $MODEL | $inst | print64=$r64 | print32=$r32" | tee "$OUT/summary.txt"
