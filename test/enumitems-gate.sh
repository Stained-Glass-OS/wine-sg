#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IShellItem::BindToHandler(BHID_EnumItems) (patches/sg/9753): a folder's items as IEnumShellItems,
# the usual way a program lists a folder through the shell since Windows 7. Wine had only BHID_SFObject,
# BHID_SFUIObject and BHID_DataObject ("Unsupported BHID {94f60519-...}", MK_E_NOOBJECT); Blender's
# Quick access list needs it. The probe (test/enumitems-probe.c) checks:
#   a folder's files and subfolders, by their parsing names, with full file system paths
#   an empty folder: an enumerator with nothing in it (Next S_FALSE)
#   a virtual folder (This PC) lists its drives
#   a file has no items (fails)
#   WINE=/opt/wine-sg/bin/wine test/enumitems-gate.sh
# Mutant (shell32/shellitem.c): SG_MUTANT_NO_BHID_ENUMITEMS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-enumitems.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/enumitems-probe.c" -lole32 -lshell32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
l() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
[ "$(l FOLDER)" = "00000000 a.txt b.txt sub" ] && pass "a folder's files and subfolder, by name" || fail "folder: '$(l FOLDER)'"
case "$(l FULL)" in 'C:\enumtest\a.txt'|'C:\enumtest\b.txt'|'C:\enumtest\sub') pass "each with its full path" ;; *) fail "full path: '$(l FULL)'" ;; esac
[ "$(l EMPTY)" = "00000000 00000001" ] && pass "an empty folder: no items" || fail "empty: '$(l EMPTY)'"
set -- $(l THISPC)
[ "${1:-}" = 00000000 ] && [ "${2:-0}" -ge 1 ] && pass "This PC lists its drives ($2)" || fail "This PC: '$(l THISPC)'"
[ "$(l FILE)" = 1 ] && pass "a file has no items" || fail "file: '$(l FILE)'"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
