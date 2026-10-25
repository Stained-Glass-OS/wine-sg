#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The fonts folder's links do not depend on a drive letter (patches/sg/6535): wineboot links each font
# to its Unix path, so removing Z: (or moving it) leaves them working, and in a prefix without Z:
# (where win32u registers the host's fonts as \??\unix\... paths) an update still links them. 6533's
# links went through dosdevices/z:, dangled once Z: was gone, an update then removed them all, and
# Krita (its own fontconfig, no font found) crashed at start (test/fontsfolder-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/fontsnoz-gate.sh
# Also win32u skips the links in the Windows directory only, not the fonts they point at in Wine's data
# directory: Tahoma and Wine's other own fonts were loaded only through the registry's Z:\ paths, and
# without Z: not at all (test/fontsnoz-probe.c).
# Mutants: SG_MUTANT_FONT_LINK_VIA_DRIVE (programs/wineboot/wineboot.c),
#          SG_MUTANT_FONT_SKIP_ALL_DIRS (dlls/win32u/font.c).
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
HOSTPATH=$(fc-list --format '%{file}\n' | grep -E '^/usr/share/fonts/.*\.(ttf|otf)$' | sort | head -n 1)
[ -n "$HOSTPATH" ] || { echo "SKIP: no host TrueType/OpenType font"; exit 77; }
HOSTFONT=$(basename "$HOSTPATH")
T=$(mktemp -d /var/tmp/sg-fontsnoz.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fontsfolder-probe.c" -lgdi32 -luser32 -ldwrite || { fail "probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fam.exe" "$HERE/fontsnoz-probe.c" -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
F="$WINEPREFIX/drive_c/windows/Fonts"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$T/fam.exe" "$WINEPREFIX/drive_c/"
fam() {
    o=$(timeout -s KILL 120 "$WINE" 'C:\fam.exe' 2>/dev/null | tr -d '\r')
    echo "      $(echo $o)"
    yes_no "$(printf '%s\n' "$o" | grep -q '^tahoma=1' && printf '%s\n' "$o" | grep -q '^marlett=1' && echo 1)" "$1"
}
run() { timeout -s KILL 120 "$WINE" 'C:\probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
links() { find "$F" -maxdepth 1 -type l | wc -l; }
dangling() { find "$F" -maxdepth 1 -xtype l | wc -l; }

# 1. a prefix with Z:: the links point at the fonts themselves, not through a drive
LINKS=$(links)
HOSTLINK=$(readlink "$F/$HOSTFONT" 2>/dev/null)
echo "      $LINKS links; $HOSTFONT -> $HOSTLINK"
yes_no "$([ "$HOSTLINK" = "$HOSTPATH" ] && echo 1)" "the link to the host's $HOSTFONT is its Unix path" "$HOSTLINK"
yes_no "$([ "$LINKS" -gt 2 ] && [ -z "$(find "$F" -maxdepth 1 -type l -lname '*dosdevices*')" ] && echo 1)" "no link goes through dosdevices"

"$WINESERVER" -k; sleep 1
fam "a new session knows Wine's own Tahoma and Marlett (the links are not loaded, the fonts are)"

# 2. Z: removed: the links still work, before any update
"$WINESERVER" -k; sleep 1
rm -f "$WINEPREFIX/dosdevices/z:"
yes_no "$([ "$(dangling)" = 0 ] && echo 1)" "with Z: removed no link dangles" "$(dangling) dangling"
out=$(run scan "$HOSTFONT"); printf '%s\n' "$out" | sed 's/^/      /'
yes_no "$(printf '%s\n' "$out" | grep -q '^host_font=1' && echo 1)" "a fontconfig-style scan still finds $HOSTFONT"
"$WINESERVER" -k; sleep 1
fam "without Z: a new session still knows Tahoma and Marlett"

# 3. an update without Z: (the host's fonts registered as \??\unix\ paths): still linked, nothing dangling
"$WINESERVER" -w
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
REG=$("$WINE" reg query 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\Fonts' 2>/dev/null | tr -d '\r' | grep -c 'unix')
echo "      after the update: $(links) links, $(dangling) dangling, $REG \\??\\unix\\ registry paths"
yes_no "$([ "$(links)" -ge "$LINKS" ] && [ "$(dangling)" = 0 ] && echo 1)" "an update without Z: keeps every font linked" "$(links) vs $LINKS"
yes_no "$([ "$(readlink "$F/$HOSTFONT")" = "$HOSTPATH" ] && echo 1)" "and $HOSTFONT's link is its Unix path"

# 4. a dangling link of ours (an old one through a drive that is gone) is made again
rm -f "$F/$HOSTFONT"; ln -s "$WINEPREFIX/dosdevices/z:$HOSTPATH" "$F/$HOSTFONT"
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
yes_no "$([ "$(readlink "$F/$HOSTFONT")" = "$HOSTPATH" ] && echo 1)" "a dangling link of ours is made again" "$(readlink "$F/$HOSTFONT")"
out=$(run scan "$HOSTFONT")
yes_no "$(printf '%s\n' "$out" | grep -q '^wine_font=1' && printf '%s\n' "$out" | grep -q '^host_font=1' && echo 1)" "the scan finds Wine's tahoma.ttf and $HOSTFONT"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
