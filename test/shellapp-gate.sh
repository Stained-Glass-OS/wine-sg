#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Shell.Application's folders, items, shortcuts and verbs (patches/sg/1645),
# 64- and 32-bit cscript: test/shellapp.js makes a folder, copies and moves
# into it (Folder.CopyHere is how scripts copy and unpack), walks FolderItems
# and FolderItemVerbs with For Each, reads and sets an item's size, type,
# date, name and extended properties, filters items, edits and saves a
# shortcut through FolderItem.GetLink, and runs a verb. All of these were
# E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/shellapp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_COPYHERE (shell32/shelldispatch.c),
# SG_MUTANT_NO_REGISTRY_VERBS (shell32/shlview_cmenu.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellapp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/shellapp.js" "$T/shellapp.js"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in "" syswow64; do
    if [ -n "$arch" ]; then cs='C:\windows\syswow64\cscript.exe'; echo "== 32-bit"; else cs=cscript; echo "== 64-bit"; fi
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$cs" //nologo 'Z:'"$(printf '%s' "$T/shellapp.js" | tr / '\\')" 2>&1 </dev/null | tr -d '\r')
    printf '%s\n' "$out" | grep -v ':fixme:' | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
