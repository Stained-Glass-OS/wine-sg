#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A context menu handler registered for an archive's ProgID and for every file (*) shows its items
# once, and its commands still find the selected items when they run (patches/sg/7500). WinRAR's
# RarExt is registered both ways and has no IPersist, so shell32's menu showed "Extract files...",
# "Extract Here" and "Extract to" twice for a .7z; and RarExt keeps the data object Initialize
# gave it without a reference of its own, so "Add to archive" and "Extract to" did nothing
# (InvokeCommand E_FAIL: the menu had released the object, no files). Windows' menu loads each
# handler once and keeps the data object while it has the handlers. The handler
# (test/menukeep-dll.c) behaves as RarExt; the probe builds the menu as File Explorer does.
#
#   WINE=/opt/wine-sg/bin/wine test/menukeep-gate.sh
# Mutants: SG_MUTANT_HANDLER_TWICE, SG_MUTANT_DROP_ITEMS_EARLY (dlls/shell32/shlview_cmenu.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-menukeep.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -shared -o "$T/sgmenukeep.dll" "$HERE/menukeep-dll.c" -lole32 -luuid -lshell32 -Wl,--kill-at || { fail "handler did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/menukeep-probe.c" -lole32 -luuid -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/sgmenukeep.dll" "$T/probe.exe" "$C/"
mkdir -p "$C/arc"
echo one > "$C/arc/a.sgkeep"; echo two > "$C/arc/b.sgkeep"; echo three > "$C/arc/c.txt"
G='{7D5A0C31-52E4-4B8F-8E21-5A4C70007500}'
reg() { timeout -s KILL 60 "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg "HKCR\\CLSID\\$G\\InprocServer32" /ve /d 'C:\sgmenukeep.dll'
reg "HKCR\\CLSID\\$G\\InprocServer32" /v ThreadingModel /d Apartment
reg 'HKCR\.sgkeep' /ve /d 'SGKeep.Archive'
reg 'HKCR\SGKeep.Archive' /ve /d 'SG keep archive'
reg "HKCR\\SGKeep.Archive\\shellex\\ContextMenuHandlers\\$G" /ve /d ''
reg 'HKCR\*\shellex\ContextMenuHandlers\SGKeep' /ve /d "$G"
cd "$C"
seen() { tr -d '\r' < "$C/menukeep-invoked.txt" 2>/dev/null; }
run() { timeout -s KILL 60 "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }

out=$(run 'C:\arc\a.sgkeep')
printf '%s\n' "$out" | grep -q '^count 1$' && pass "registered for its ProgID and for *: its item shows once" \
    || fail "item count for a .sgkeep file: $(printf '%s\n' "$out" | grep count)"
printf '%s\n' "$out" | grep -q '^invoke 00000000$' && seen | grep -q '^invoked 1: C:\\arc\\a.sgkeep$' \
    && pass "...and its command, run after the menu was built, still finds the item" \
    || fail "invoke: $(printf '%s\n' "$out" | grep invoke), handler saw: $(seen)"
rm -f "$C/menukeep-invoked.txt"

out=$(run 'C:\arc\c.txt')
printf '%s\n' "$out" | grep -q '^count 1$' && printf '%s\n' "$out" | grep -q '^invoke 00000000$' \
    && seen | grep -q '^invoked 1: C:\\arc\\c.txt$' \
    && pass "a file of another type (registered for * only): once, and it runs" \
    || fail "c.txt: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen)"
rm -f "$C/menukeep-invoked.txt"

out=$(run 'C:\arc\a.sgkeep' 'C:\arc\b.sgkeep')
printf '%s\n' "$out" | grep -q '^count 1$' && printf '%s\n' "$out" | grep -q '^invoke 00000000$' \
    && seen | grep -q '^invoked 2: C:\\arc\\a.sgkeep C:\\arc\\b.sgkeep$' \
    && pass "two selected archives: one item, and it gets both" \
    || fail "two items: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
