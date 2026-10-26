#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A file's right-click menu has the verbs every such item has, not only its own type's, and
# cascading verbs (patches/sg/7502), as Windows merges them: HKCR\*\shell (every file -- VS
# Code's "Open with Code"), SystemFileAssociations\.ext\shell, AllFilesystemObjects\shell,
# Directory\shell and Folder\shell for folders; a verb with SubCommands (the CommandStore's
# verbs, with CommandFlags separators) or ExtendedSubCommandsKey is a submenu -- PeaZip's whole
# menu is such a cascade and showed nothing. Extended verbs show with Shift only, LegacyDisable
# and ProgrammaticAccessOnly never; a verb the menu already has is not added again. The probe
# builds the menu as File Explorer does and runs items; the verbs start a probe copy that
# writes its arguments.
#
#   WINE=/opt/wine-sg/bin/wine test/menuverbs-gate.sh
# Mutants: SG_MUTANT_NO_SHARED_VERBS, SG_MUTANT_NO_CASCADE (dlls/shell32/shlview_cmenu.c).
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
T=$(mktemp -d /var/tmp/sg-menuverbs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/menuverbs-probe.c" -lole32 -luuid -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/probe.exe"; cp "$T/probe.exe" "$C/args.exe"
mkdir -p "$C/w/folder"; echo hi > "$C/w/note.sgv"
cat > "$T/verbs.reg" <<'R'
REGEDIT4

[HKEY_CLASSES_ROOT\.sgv]
@="SGV.File"

[HKEY_CLASSES_ROOT\SGV.File\shell\open\command]
@="C:\\args.exe own \"%1\""

[HKEY_CLASSES_ROOT\*\shell\SGStar]
@="SG Star Verb"

[HKEY_CLASSES_ROOT\*\shell\SGStar\command]
@="C:\\args.exe star \"%1\""

[HKEY_CLASSES_ROOT\*\shell\SGShift]
@="SG Shift Verb"
"Extended"=""

[HKEY_CLASSES_ROOT\*\shell\SGShift\command]
@="C:\\args.exe shift \"%1\""

[HKEY_CLASSES_ROOT\*\shell\SGOff]
@="SG Off Verb"
"LegacyDisable"=""

[HKEY_CLASSES_ROOT\*\shell\SGOff\command]
@="C:\\args.exe off \"%1\""

[HKEY_CLASSES_ROOT\SystemFileAssociations\.sgv\shell\SGSfa]
"MUIVerb"="SG Ext Verb"

[HKEY_CLASSES_ROOT\SystemFileAssociations\.sgv\shell\SGSfa\command]
@="C:\\args.exe sfa \"%1\""

[HKEY_CLASSES_ROOT\AllFilesystemObjects\shell\SGStar]
@="SG Star Verb"

[HKEY_CLASSES_ROOT\AllFilesystemObjects\shell\SGStar\command]
@="C:\\args.exe allfs-star \"%1\""

[HKEY_CLASSES_ROOT\AllFilesystemObjects\shell\SGAll]
@="SG All Verb"

[HKEY_CLASSES_ROOT\AllFilesystemObjects\shell\SGAll\command]
@="C:\\args.exe all \"%1\""

[HKEY_CLASSES_ROOT\Directory\shell\SGDir]
@="SG Dir Verb"

[HKEY_CLASSES_ROOT\Directory\shell\SGDir\command]
@="C:\\args.exe dir \"%1\""

[HKEY_CLASSES_ROOT\*\shell\SGCascade]
"MUIVerb"="SG Cascade"
"SubCommands"="SG.Test.One; SG.Test.Two;"

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Test.One]
"MUIVerb"="SG One"

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Test.One\command]
@="C:\\args.exe one \"%1\""

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Test.Two]
@="SG Two"
"CommandFlags"=dword:00000020

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\SG.Test.Two\command]
@="C:\\args.exe two \"%1\""

[HKEY_CLASSES_ROOT\Directory\shell\SGMore]
"MUIVerb"="SG More"
"SubCommands"=""
"ExtendedSubCommandsKey"="SGMoreMenu"

[HKEY_CLASSES_ROOT\SGMoreMenu\shell\first]
"MUIVerb"="SG First"

[HKEY_CLASSES_ROOT\SGMoreMenu\shell\first\command]
@="C:\\args.exe first \"%1\""
R
cp "$T/verbs.reg" "$C/verbs.reg"
timeout -s KILL 60 "$WINE" regedit /S 'C:\verbs.reg' >/dev/null 2>&1
"$WINESERVER" -w
cd "$C"
run() { timeout -s KILL 60 "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
seen() { [ -f "$C/menuverbs-args.txt" ] && tr -d '\r' < "$C/menuverbs-args.txt"; }
cnt() { printf '%s\n' "$out" | grep -cx "$1"; }

out=$(run 'C:\w\note.sgv')
[ "$(cnt 'item SG Star Verb')" = 1 ] && [ "$(cnt 'item SG Ext Verb')" = 1 ] && [ "$(cnt 'item SG All Verb')" = 1 ] \
    && pass "a file's menu has the *, SystemFileAssociations and AllFilesystemObjects verbs (a verb of two of them once)" \
    || fail "file verbs: $(printf '%s\n' "$out" | grep -E 'SG ' | tr '\n' '|')"
[ "$(cnt 'item SG Shift Verb')" = 0 ] && [ "$(cnt 'item SG Off Verb')" = 0 ] && [ "$(cnt 'item SG Dir Verb')" = 0 ] \
    && pass "...not an Extended verb (no Shift), a LegacyDisable one, or a folder's" \
    || fail "hidden verbs shown: $(printf '%s\n' "$out" | grep -E 'Shift|Off|Dir' | tr '\n' '|')"
printf '%s\n' "$out" | grep -A4 -x 'item SG Cascade >' | tr '\n' '|' | grep -q '^item SG Cascade >|sub SG One|subsep|sub SG Two|' \
    && pass "a SubCommands cascade: a submenu of the CommandStore's verbs, with CommandFlags' separator" \
    || fail "cascade: $(printf '%s\n' "$out" | grep -A4 'SG Cascade' | tr '\n' '|')"
out=$(run 'C:\w\note.sgv' --extended)
[ "$(cnt 'item SG Shift Verb')" = 1 ] && pass "with Shift the Extended verb shows" || fail "extended: $(printf '%s\n' "$out" | grep Shift)"

rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\note.sgv' --invoke 'SG Star Verb')
seen | grep -qxF 'args[star "C:\w\note.sgv"]' && printf '%s\n' "$out" | grep -qx 'verb SGStar' \
    && pass "a * verb runs its own command (verb SGStar)" || fail "star: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen)"
rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\note.sgv' --invoke 'SG Two')
seen | grep -qxF 'args[two "C:\w\note.sgv"]' && printf '%s\n' "$out" | grep -qx 'verb SG.Test.Two' \
    && pass "a cascade's command runs the CommandStore's command" || fail "subcommand: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen)"
rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\note.sgv' --invoke 'SG Ext Verb')
seen | grep -qxF 'args[sfa "C:\w\note.sgv"]' && pass "a SystemFileAssociations verb runs" || fail "sfa: saw $(seen)"
rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\note.sgv' --invoke '&Open')
seen | grep -qxF 'args[own "C:\w\note.sgv"]' && pass "the type's own Open still runs" || fail "own open: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen)"

out=$(run 'C:\w\folder')
[ "$(cnt 'item SG Dir Verb')" = 1 ] && [ "$(cnt 'item SG All Verb')" = 1 ] && [ "$(cnt 'item SG Star Verb')" = 1 ] \
    && pass "a folder's menu has the Directory and AllFilesystemObjects verbs" \
    || fail "folder verbs: $(printf '%s\n' "$out" | grep -E 'SG ' | tr '\n' '|')"
printf '%s\n' "$out" | grep -A1 -x 'item SG More >' | tr '\n' '|' | grep -q '^item SG More >|sub SG First|' \
    && pass "an ExtendedSubCommandsKey cascade: a submenu of that class's verbs" \
    || fail "extended subcommands: $(printf '%s\n' "$out" | grep -A2 'SG More' | tr '\n' '|')"
rm -f "$C/menuverbs-args.txt"
out=$(run 'C:\w\folder' --invoke 'SG First')
seen | grep -qxF 'args[first "C:\w\folder"]' && pass "...and its command runs" || fail "first: $(printf '%s\n' "$out" | tail -2 | tr '\n' ' ') saw: $(seen)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
