#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Control Panel's category view (patches/sg/9860). Windows names FOLDERID_ControlPanelFolder
# "::{26EE0668-A00A-44D7-9371-BEB064C98683}\0": the category view {26EE0668-...} and in it "0", All
# Control Panel Items. Launchers that list the Control Panel parse that name (Wox 2.5, Open-Shell 4.4);
# shell32 had no class {26EE0668-...}, so CoCreateInstance said REGDB_E_CLASSNOTREG, parsing failed and
# their Control Panel entries were missing.
#   class       CoCreateInstance({26EE0668-...}, IShellFolder): S_OK
#   parse       SHParseDisplayName("::{26EE0668-...}\0"): S_OK, named "All Control Panel Items", parsing
#               name "::{26EE0668-A00A-44D7-9371-BEB064C98683}\0"
#   items       it lists as many items as FOLDERID_ControlPanelFolder does (more than none), and its first
#               item has a name through IShellItem (its parents bind)
#   WINE=/opt/wine-sg/bin/wine test/cpanelcategory-gate.sh
# Mutant (shell32/cpanelfolder.c): SG_MUTANT_NO_CPANEL_CATEGORY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cpanelcategory.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/cpanelcategory-probe.c" -lole32 -lshell32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p" | head -1; }
printf '%s\n' "$out" | grep -qx END || fail "the probe did not finish"
[ "$(v COCREATE)" = 0x00000000 ] && pass "the category view is a class" || fail "CoCreateInstance: '$(v COCREATE)'"
[ "$(v PARSE)" = 0x00000000 ] && pass "::{26EE0668-...}\\0 parses" || fail "SHParseDisplayName: '$(v PARSE)'"
[ "$(v NAME)" = "0x00000000 All Control Panel Items" ] && pass "named All Control Panel Items" || fail "name: '$(v NAME)'"
p=$(v PARSING | tr 'a-f' 'A-F')
[ "$p" = '0x00000000 ::{26EE0668-A00A-44D7-9371-BEB064C98683}\0' ] && pass "its parsing name is FOLDERID_ControlPanelFolder's" \
    || fail "parsing name: '$(v PARSING)'"
k=$(v KENUM | cut -d' ' -f2); n=$(v ENUM | cut -d' ' -f2)
[ -n "$n" ] && [ "$n" -gt 0 ] 2>/dev/null && [ "$n" = "$k" ] && pass "it lists the Control Panel's $n items" \
    || fail "items: parsed '$(v ENUM)', known folder '$(v KENUM)'"
case "$(v ITEM)" in 0x00000000\ ?*) pass "an item's name: $(v ITEM | cut -d' ' -f2-)" ;; *) fail "item: '$(v ITEM)'" ;; esac
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
