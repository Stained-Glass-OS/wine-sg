#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Quick access as a shell folder, ::{679F85CB-0220-4080-B29B-5540CC05AAB6} (patches/sg/9753).
# Blender adds Windows' Quick access to its file browser's side bar ("shell:::{679f85cb-...}",
# BHID_EnumItems, each item's SIGDN_FILESYSPATH); Wine had no such folder ("DllGetClassObject failed
# for CLSID={679f85cb-...}"). The probe (test/quickaccess-probe.c) checks:
#   the folder parses and is called "Quick access"
#   its items are the pinned folders (by default Desktop, Downloads, Documents, Pictures), each a file
#   system folder with its own path and its own name ("Documents")
#   a folder pinned in File Explorer's list (HKCU\Software\Stained Glass\Explorer\QuickAccess, Pinned)
#   follows the pins, a frequent one (Frequent, visited 3 times) comes after them
#   an item opens as its folder: the pinned folder's file is listed through it
#   WINE=/opt/wine-sg/bin/wine test/quickaccess-gate.sh
# Mutant (shell32/sendto.c): SG_MUTANT_QUICK_ACCESS_EMPTY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-quickaccess.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/quickaccess-probe.c" -lole32 -lshell32 -luuid -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out" | head -30
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
docs=$(printf '%s\n' "$out" | sed -n 's/^KNOWN //p')
items=$(printf '%s\n' "$out" | awk '$1 == "ITEM"' | sed 's/^ITEM //')
pinned=$(printf '%s\n' "$out" | awk '$1 == "PINNED"' | sed 's/^PINNED //')
[ "$(printf '%s\n' "$out" | sed -n 's/^PARSE //p')" = 00000000 ] && pass "shell:::{679f85cb-...} parses" || fail "parse: $(printf '%s\n' "$out" | sed -n 's/^PARSE //p')"
[ "$(printf '%s\n' "$out" | sed -n 's/^NAME //p')" = "Quick access" ] && pass "it is called Quick access" || fail "name: '$(printf '%s\n' "$out" | sed -n 's/^NAME //p')'"
n=$(printf '%s\n' "$items" | grep -c ' 1 1 ')
[ "$n" -ge 4 ] && pass "$n items, each a file system folder" || fail "items: $(printf '%s|' "$items")"
printf '%s\n' "$items" | grep -qF "$docs 1 1 Documents" && pass "Documents is there with its path and its name" || fail "no '$docs ... Documents' among the items"
first=$(printf '%s\n' "$pinned" | sed -n 1p); second=$(printf '%s\n' "$pinned" | sed -n 2p); third=$(printf '%s\n' "$pinned" | sed -n 3p)
case "$first" in "$docs "*) ;; *) first=bad ;; esac
[ "$first" != bad ] && case "$second" in 'C:\qa-pinned 1 1 '*) true ;; *) false ;; esac && pass "a folder pinned in File Explorer follows the pins" || fail "pinned: $(printf '%s|' "$pinned")"
case "$third" in 'C:\qa-freq 1 1 '*) pass "a frequent folder comes after the pins" ;; *) fail "frequent: '$third'" ;; esac
printf '%s\n' "$out" | grep -qx 'INSIDE inside.txt' && pass "an item opens as its folder" || fail "the pinned folder's contents were not listed"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
