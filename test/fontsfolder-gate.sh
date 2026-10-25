#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The system's fonts folder holds the fonts (patches/sg/6533): wineboot links Wine's own fonts and the
# host's (each font win32u registered with a full path) into C:\windows\Fonts, so that programs that
# read the folder themselves (fontconfig: Inkscape, GIMP, Krita) find them; win32u does not load the
# links again, so GDI and DirectWrite count the same families as without them; an update removes only
# its own links (for fonts gone) and never a file put there by someone else (test/fontsfolder-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/fontsfolder-gate.sh
#   INKSCAPE_PREFIX=/path/to/a/prefix/with/Inkscape: also an Inkscape text export there (optional)
# Mutants: SG_MUTANT_NO_FONT_LINKS (programs/wineboot), SG_MUTANT_FONT_LINKS_LOADED (dlls/win32u font.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" fc-list; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
# a host font that fontconfig knows (TrueType/OpenType, under /usr/share/fonts)
HOSTPATH=$(fc-list --format '%{file}\n' | grep -E '^/usr/share/fonts/.*\.(ttf|otf)$' | sort | head -n 1)
[ -n "$HOSTPATH" ] || { echo "SKIP: no host TrueType/OpenType font"; exit 77; }
HOSTFONT=$(basename "$HOSTPATH")
T=$(mktemp -d /var/tmp/sg-fontsfolder.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fontsfolder-probe.c" -lgdi32 -luser32 -ldwrite || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
F="$WINEPREFIX/drive_c/windows/Fonts"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
run() { timeout -s KILL 120 "$WINE" 'C:\probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}

out=$(run scan "$HOSTFONT"); printf '%s\n' "$out" | sed 's/^/      /'
check wine_font "a fontconfig-style scan of C:\\windows\\Fonts finds Wine's tahoma.ttf, readable"
check host_font "and the host's $HOSTFONT"
LINKS=$(find "$F" -maxdepth 1 -type l | wc -l)
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }
yes_no "$([ "$LINKS" -gt 2 ] && [ "$(find "$F" -maxdepth 1 -type f | wc -l)" = 0 ] && echo 1)" "the folder holds symbolic links only, no copies" "$LINKS links"

# the same families with and without the links (a new wineserver each time: the font cache is rebuilt)
"$WINESERVER" -k; sleep 1
with=$(run count); "$WINESERVER" -k; sleep 1
mkdir "$T/away"; find "$F" -maxdepth 1 -type l -exec mv {} "$T/away/" \;
without=$(run count); "$WINESERVER" -k; sleep 1
mv "$T/away/"* "$F/"
printf '      with links: %s\n      without:    %s\n' "$with" "$without"
yes_no "$([ -n "$with" ] && [ "$with" = "$without" ] && echo 1)" "GDI and DirectWrite count the same families with the links as without"
REG=$("$WINE" reg query 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\Fonts' 2>/dev/null | tr -d '\r' | grep -ci "$(basename "$HOSTPATH" | sed 's/\./\\./')")
yes_no "$([ "$REG" = 1 ] && echo 1)" "the registry lists $HOSTFONT once, at its own path" "$REG entries"

# an update: a real file someone put there stays, a link for a font that is gone goes, nothing doubles
printf "not a font: a file someone else put here\n" > "$F/userfont.ttf"
"$WINE" reg add 'HKLM\Software\Stained Glass\Fonts\Links' /v gone.ttf /d 'Z:\nonexistent\gone.ttf' /f >/dev/null 2>&1
ln -s "$WINEPREFIX/dosdevices/z:/nonexistent/gone.ttf" "$F/gone.ttf"
"$WINESERVER" -w
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
yes_no "$([ -f "$F/userfont.ttf" ] && [ ! -L "$F/userfont.ttf" ] && echo 1)" "an update leaves a file it did not make"
yes_no "$([ ! -e "$F/gone.ttf" ] && [ ! -L "$F/gone.ttf" ] && echo 1)" "and removes its link for a font that is gone"
yes_no "$([ "$(find "$F" -maxdepth 1 -type l | wc -l)" = "$LINKS" ] && echo 1)" "and makes no second link for any font" "$(find "$F" -maxdepth 1 -type l | wc -l) vs $LINKS"

# optional: Inkscape (fontconfig, Pango) draws text in a prefix that has it
if [ -n "${INKSCAPE_PREFIX:-}" ] && command -v convert >/dev/null; then
    printf '<svg xmlns="http://www.w3.org/2000/svg" width="200" height="60"><rect width="200" height="60" fill="#2060c0"/><text x="10" y="40" font-size="28" fill="white">Hello</text></svg>' > "$T/t.svg"
    WINEPREFIX="$INKSCAPE_PREFIX" timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1
    WINEPREFIX="$INKSCAPE_PREFIX" timeout -s KILL 300 "$WINE" 'C:\Program Files\Inkscape\bin\inkscape.com' --export-type=png \
        "--export-filename=Z:$(echo "$T/t.png" | tr / '\\\\')" "Z:$(echo "$T/t.svg" | tr / '\\\\')" >/dev/null 2>&1
    WHITE=$(convert "$T/t.png" txt:- 2>/dev/null | grep -c '#FFFFFF')
    yes_no "$([ "${WHITE:-0}" -gt 20 ] && echo 1)" "Inkscape draws the text (fontconfig finds the fonts)" "$WHITE white pixels"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
