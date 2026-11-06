#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Other programs' right-click menu entries (patches/sg/0779): a context menu
# handler registered under shellex\ContextMenuHandlers (as TortoiseGit's,
# 7-Zip's, "Edit with Notepad++") adds its item to shell32's menu for a file
# (registered for *) and for a folder (Directory); its verb and help come from
# it, and its item runs it -- by its command, and by its verb. A test handler
# (menuext-dll.c) and a probe asking for the menu as File Explorer does.
# David 2026-10-03: TortoiseGit installed but added nothing to the menu.
#
#   WINE=/opt/wine-sg/bin/wine test/menuext-gate.sh   (mutant SG_MUTANT_NO_MENU_HANDLERS)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-menuext.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -shared -o "$T/sgmenuext.dll" "$HERE/menuext-dll.c" -lole32 -luuid -lshell32 -Wl,--kill-at || { fail "handler did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/menuext-probe.c" -lole32 -luuid -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/sgmenuext.dll" "$T/probe.exe" "$C/"
echo hello > "$C/notes.txt"; mkdir -p "$C/somefolder"
G='{5C1E7A10-3B2D-4F6E-9A11-2B770DE45102}'
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg "HKCR\\CLSID\\$G\\InprocServer32" /ve /d 'C:\sgmenuext.dll'
reg "HKCR\\CLSID\\$G\\InprocServer32" /v ThreadingModel /d Apartment
reg 'HKCR\*\shellex\ContextMenuHandlers\SGTest' /ve /d "$G"
reg 'HKCR\Directory\shellex\ContextMenuHandlers\SGTest' /ve /d "$G"
cd "$C"
out=$("$WINE" probe.exe 'C:\notes.txt' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q '^item [0-9]* SG Test Action$' && pass "a file's menu has the handler's item" \
    || { fail "no handler item for a file: $(printf '%s\n' "$out" | grep '^item' | tr '\n' ' ' | cut -c1-300)"; }
printf '%s\n' "$out" | grep -q '^verb sgtest$' && printf '%s\n' "$out" | grep -q '^help SG test help$' \
    && pass "...its verb and help are the handler's" || fail "verb/help: $(printf '%s\n' "$out" | grep -E '^(verb|help)')"
printf '%s\n' "$out" | grep -q '^invoke-id 00000000$' && printf '%s\n' "$out" | grep -q '^invoke-verb 00000000$' \
    && [ "$(grep -c 'notes.txt' "$C/menuext-invoked.txt" 2>/dev/null)" = 2 ] \
    && pass "...and it runs the handler, by its command and by its verb, on that file" \
    || fail "invoke: $(printf '%s\n' "$out" | grep '^invoke'), wrote: $(cat "$C/menuext-invoked.txt" 2>/dev/null)"
pos_test=$(printf '%s\n' "$out" | grep -n '^item [0-9]* SG Test Action$' | cut -d: -f1)
pos_send=$(printf '%s\n' "$out" | grep -n '^item -1 Se&nd to$\|^item [0-9-]* Se&nd to$' | cut -d: -f1)
[ -n "$pos_test" ] && [ -n "$pos_send" ] && [ "$pos_test" -lt "$pos_send" ] && pass "...placed before Send to, as Windows" \
    || fail "placed at $pos_test, Send to at $pos_send"
rm -f "$C/menuext-invoked.txt"
out=$("$WINE" probe.exe 'C:\somefolder' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q '^item [0-9]* SG Test Action$' && printf '%s\n' "$out" | grep -q '^invoke-id 00000000$' \
    && grep -q 'somefolder' "$C/menuext-invoked.txt" 2>/dev/null \
    && pass "a folder's menu has it too (registered for Directory), and it runs" || fail "folder: $(printf '%s\n' "$out" | tr '\n' ' ' | cut -c1-300)"
# blocked (Shell Extensions\Blocked): not loaded
reg 'HKLM\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked' /v "$G" /d blocked
out=$("$WINE" probe.exe 'C:\notes.txt' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q 'SG Test Action' && fail "a blocked handler still loaded" || pass "a handler under Shell Extensions\\Blocked is not loaded"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
