#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The core families in DirectWrite (patches/sg/7715): Arial, Times New Roman, Courier New, Segoe UI,
# Calibri, Cambria and Consolas are families of the system collection by themselves, as their open
# metric-compatible fonts (Liberation, Inter, Carlito, Caladea, Cascadia Mono), keeping the fonts' own
# names -- without HKCU\Software\Wine\Fonts\Replacements. WPF's fallback looks "Arial" up by name
# (FindFamilyName) and fails fast when it is not there: DuckDuckGo, MyMoney.Net and LanguageTool
# stopped at their first text in a profile without sg-shell's Replacements (2026-11-08).
# A Replacements entry the person set still wins.
#   WINE=/opt/wine-sg/bin/wine test/dwcore-gate.sh
# Mutant: SG_MUTANT_DW_NO_CORE (dlls/dwrite/font.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" fc-list; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list : family | grep -qx 'Liberation Sans' || { echo "SKIP: no Liberation fonts on the host"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwcore.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/dwcore-probe.c" -ldwrite || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/"
run() { (cd "$C" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' "$@" 2>/dev/null | tr -d '\r'); }
has() { if printf '%s\n' "$1" | grep -qxF "$2"; then pass "$3"; else fail "$3: no '$2' in: $(printf '%s' "$1" | tr '\n' ' ')"; fi; }
"$WINE" reg delete 'HKCU\Software\Wine\Fonts\Replacements' /f >/dev/null 2>&1
"$WINE" reg query 'HKCU\Software\Wine\Fonts\Replacements' >/dev/null 2>&1 && { fail "a Replacements key is there"; exit 1; }
pass "no Replacements key in this profile"
out=$(run Arial "Times New Roman" "Courier New" "Segoe UI")
has "$out" "Found=Arial 1" "Arial is a family of the system collection (WPF's fallback finds it)"
has "$out" "FontOwn=Arial Liberation Sans" "as Liberation Sans, under the font's own name"
has "$out" "HasA=Arial 1" "which has the letters"
has "$out" "Found=Times New Roman 1" "Times New Roman too"
has "$out" "FontOwn=Times New Roman Liberation Serif" "as Liberation Serif"
has "$out" "Found=Courier New 1" "Courier New too"
has "$out" "FontOwn=Courier New Liberation Mono" "as Liberation Mono"
has "$out" "Found=Segoe UI 1" "and Segoe UI (the interface font)"
out=$(run NoSuchFamilyAnywhere)
has "$out" "Found=NoSuchFamilyAnywhere 0" "a name no font has is still not found"
"$WINE" reg add 'HKCU\Software\Wine\Fonts\Replacements' /v Arial /d 'DejaVu Sans' /f >/dev/null 2>&1
"$(dirname "$WINE")/wineserver" -w
out=$(run Arial)
if fc-list : family | grep -qx 'DejaVu Sans'; then
    has "$out" "FontOwn=Arial DejaVu Sans" "a Replacements entry the person set still wins"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
