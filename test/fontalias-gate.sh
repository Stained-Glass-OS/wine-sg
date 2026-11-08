#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The file names of the core fonts in the fonts folder (patches/sg/7711): wineboot links arial*.ttf,
# times*.ttf and cour*.ttf in C:\windows\Fonts to the system's Liberation fonts, so that programs that
# open the core fonts by file name find them -- Java's runtime on Windows takes its logical fonts from
# ARIAL.TTF, TIMES.TTF and COUR.TTF, and without them drew every letter as a box (Charles, 2026-11-08).
# Only where the folder has no such file; recorded like 6533's links (win32u loads them once: the
# family counts do not change); a real font put there later replaces ours and stays.
#   WINE=/opt/wine-sg/bin/wine test/fontalias-gate.sh
# Mutants: SG_MUTANT_NO_FONT_ALIASES, SG_MUTANT_FONT_ALIAS_KEEP_FILE (programs/wineboot).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }
for t in "$MINGW" fc-list; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list : family | grep -qx 'Liberation Sans' || { echo "SKIP: no Liberation fonts on the host"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fontalias.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fontsfolder-probe.c" -lgdi32 -luser32 -ldwrite || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
F="$WINEPREFIX/drive_c/windows/Fonts"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
run() { timeout -s KILL 120 "$WINE" 'C:\probe.exe' "$@" 2>/dev/null | tr -d '\r'; }

for f in arial.ttf arialbd.ttf ariali.ttf arialbi.ttf times.ttf timesbd.ttf cour.ttf courbi.ttf; do
    ok=$(run scan "$f" | grep -c '^host_font=1')
    tgt=$(readlink "$F/$f" 2>/dev/null)
    yes_no "$([ "$ok" = 1 ] && [ -L "$F/$f" ] && echo 1)" "C:\\windows\\Fonts\\$f is a readable font (a link to $(basename "${tgt:-none}"))"
done
case "$(readlink "$F/arial.ttf")" in *LiberationSans-Regular.ttf) pass "arial.ttf is Liberation Sans";; *) fail "arial.ttf -> $(readlink "$F/arial.ttf")";; esac
case "$(readlink "$F/timesbd.ttf")" in *LiberationSerif-Bold.ttf) pass "timesbd.ttf is Liberation Serif Bold";; *) fail "timesbd.ttf -> $(readlink "$F/timesbd.ttf")";; esac
case "$(readlink "$F/cour.ttf")" in *LiberationMono-Regular.ttf) pass "cour.ttf is Liberation Mono";; *) fail "cour.ttf -> $(readlink "$F/cour.ttf")";; esac
REC=$("$WINE" reg query 'HKLM\Software\Stained Glass\Fonts\Links' /v arial.ttf 2>/dev/null | tr -d '\r' | grep -c 'REG_SZ')
yes_no "$([ "$REC" = 1 ] && echo 1)" "recorded with the other links (win32u does not load it again)"

# the same families with and without them
"$WINESERVER" -k; sleep 1
with=$(run count); "$WINESERVER" -k; sleep 1
mkdir "$T/away"; for f in arial arialbd ariali arialbi times timesbd timesi timesbi cour courbd couri courbi; do mv "$F/$f.ttf" "$T/away/" 2>/dev/null; done
without=$(run count); "$WINESERVER" -k; sleep 1
mv "$T/away/"* "$F/"
printf '      with: %s\n      without: %s\n' "$with" "$without"
yes_no "$([ -n "$with" ] && [ "$with" = "$without" ] && echo 1)" "GDI and DirectWrite count the same families with them as without"

# a real font put in place of one of ours stays and is no longer ours; a missing one comes back
rm -f "$F/times.ttf"; cp "$(fc-list --format '%{file}\n' 'Liberation Sans:style=Regular' | head -1)" "$F/times.ttf"
rm -f "$F/cour.ttf"
"$WINESERVER" -w
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
yes_no "$([ -f "$F/times.ttf" ] && [ ! -L "$F/times.ttf" ] && echo 1)" "an update leaves a real times.ttf someone put there"
REC=$("$WINE" reg query 'HKLM\Software\Stained Glass\Fonts\Links' /v times.ttf 2>/dev/null | tr -d '\r' | grep -c 'REG_SZ')
yes_no "$([ "$REC" = 0 ] && echo 1)" "and it is not ours any more (win32u loads it)" "$REC records"
yes_no "$([ -L "$F/cour.ttf" ] && echo 1)" "a link of ours that went is made again"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
