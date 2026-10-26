#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder's background menu (right-click on the empty part of a folder or of the desktop) has
# the programs' verbs and handlers for it (patches/sg/7503), as Windows: Directory\Background
# (and DesktopBackground on the desktop) shell verbs -- "Open with Code", "Git Bash Here",
# PeaZip's "Browse path with PeaZip" -- with cascades, and its shellex\ContextMenuHandlers
# (TortoiseGit), before New. A verb runs on the folder, in the folder; commands get the folder
# by %V (and %v, as Git's "--cd=%v."), its parent by %W. Before, the background menu had only
# shell32's own items. The probe builds the menu as File Explorer does (CreateViewObject).
#
#   WINE=/opt/wine-sg/bin/wine test/menubg-gate.sh
# Mutants: SG_MUTANT_NO_BACKGROUND_VERBS (dlls/shell32/shlview_cmenu.c),
#          SG_MUTANT_NO_PERCENT_V (dlls/shell32/shlexec.c).
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
T=$(mktemp -d /var/tmp/sg-menubg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/menuverbs-probe.c" -lole32 -luuid -lshell32 || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -shared -o "$T/sgmenukeep.dll" "$HERE/menukeep-dll.c" -lole32 -luuid -lshell32 -Wl,--kill-at || { fail "handler did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/probe.exe"; cp "$T/probe.exe" "$C/args.exe"; cp "$T/sgmenukeep.dll" "$C/"
mkdir -p "$C/w/folder"
cat > "$C/bg.reg" <<'R'
REGEDIT4

[HKEY_CLASSES_ROOT\Directory\Background\shell\SGHere]
@="SG Here"

[HKEY_CLASSES_ROOT\Directory\Background\shell\SGHere\command]
@="C:\\args.exe here \"%V\" --cd=%v. up=%W"

[HKEY_CLASSES_ROOT\Directory\Background\shell\SGBgCascade]
"MUIVerb"="SG Bg Cascade"
"SubCommands"="SG.Bg.One"

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Bg.One]
"MUIVerb"="SG Bg One"

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Bg.One\command]
@="C:\\args.exe bgone \"%V\""

[HKEY_CLASSES_ROOT\DesktopBackground\shell\SGDesk]
@="SG Desk"

[HKEY_CLASSES_ROOT\DesktopBackground\shell\SGDesk\command]
@="C:\\args.exe desk"

[HKEY_CLASSES_ROOT\CLSID\{7D5A0C31-52E4-4B8F-8E21-5A4C70007500}\InprocServer32]
@="C:\\sgmenukeep.dll"
"ThreadingModel"="Apartment"

[HKEY_CLASSES_ROOT\Directory\Background\shellex\ContextMenuHandlers\SGKeep]
@="{7D5A0C31-52E4-4B8F-8E21-5A4C70007500}"
R
timeout -s KILL 60 "$WINE" regedit /S 'C:\bg.reg' >/dev/null 2>&1
"$WINESERVER" -w
cd "$C"
run() { timeout -s KILL 60 "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
seen() { [ -f "$C/$1" ] && tr -d '\r' < "$C/$1"; }
cnt() { printf '%s\n' "$out" | grep -cx "$1"; }

out=$(run 'C:\w\folder' --background)
[ "$(cnt 'item SG Here')" = 1 ] && [ "$(cnt 'item SG Keep Action')" = 1 ] && [ "$(cnt 'item SG Desk')" = 0 ] \
    && pass "a folder's background menu has the Directory\\Background verb and handler (not DesktopBackground's)" \
    || fail "background items: $(printf '%s\n' "$out" | tr '\n' '|')"
printf '%s\n' "$out" | grep -A1 -x 'item SG Bg Cascade >' | tr '\n' '|' | grep -q '^item SG Bg Cascade >|sub SG Bg One|' \
    && pass "...and a cascade" || fail "background cascade: $(printf '%s\n' "$out" | grep -A2 Cascade | tr '\n' '|')"
here=$(printf '%s\n' "$out" | grep -n -x 'item SG Here' | cut -d: -f1); new=$(printf '%s\n' "$out" | grep -n -x 'item New >' | cut -d: -f1)
[ -n "$here" ] && [ -n "$new" ] && [ "$here" -lt "$new" ] && [ "$(cnt 'item New >')" = 1 ] \
    && pass "...before New, and New once" || fail "order: here=$here new=$new"

rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\folder' --background --invoke 'SG Here')
seen menuverbs-args.txt | grep -qxF 'args[here "C:\w\folder" --cd=C:\w\folder. up=C:\w]' \
    && pass "a background verb runs on the folder: %V and %v the folder, %W its parent" \
    || fail "here: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen menuverbs-args.txt)"
rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\folder' --background --invoke 'SG Bg One')
seen menuverbs-args.txt | grep -qxF 'args[bgone "C:\w\folder"]' && pass "a background cascade's command runs" \
    || fail "bgone: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen menuverbs-args.txt)"
out=$(run 'C:\w\folder' --background --invoke 'SG Keep Action')
seen menukeep-invoked.txt | grep -qxF 'background C:\w\folder' && pass "a background handler is given the folder, and runs" \
    || fail "handler: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen menukeep-invoked.txt)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
