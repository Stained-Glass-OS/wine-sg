#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Files dropped on a file in File Explorer or on the desktop go where Windows sends them
# (patches/sg/7501): to the file type's drop handler (HKCR\<ProgID>\shellex\DropHandler --
# WinRAR adds them to the .rar), to a program (exefile/batfile: started with the files as its
# arguments, quoted when they have spaces), through a shortcut (lnkfile: its program with the
# shortcut's arguments and then the files; its folder takes them in), into a .zip (a Compressed
# Folder). Before, every drop on a file went to the folder it was in: dropping a file on
# numbers.rar moved the file next to it. A file with no handler still leaves the drop to its
# folder. The probe drops as File Explorer does (the item's GetUIObjectOf(IDropTarget), the
# sources' IDataObject, DragEnter/DragOver/Drop); test/filedrop-handler.c is an archiver-like
# handler.
#
#   WINE=/opt/wine-sg/bin/wine test/filedrop-gate.sh
# Mutant: SG_MUTANT_NO_FILE_DROP (dlls/shell32/droptarget.c).
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
T=$(mktemp -d /var/tmp/sg-filedrop.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -shared -o "$T/sgdrophandler.dll" "$HERE/filedrop-handler.c" -lole32 -luuid -lshell32 -Wl,--kill-at || { fail "handler did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/filedrop-probe.c" -lole32 -luuid -lshell32 -lshlwapi || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/sgdrophandler.dll" "$T/probe.exe" "$C/"
mkdir -p "$C/src dir" "$C/tgt" "$C/into"
echo one > "$C/src dir/a b.txt"; echo two > "$C/src dir/c.txt"; echo three > "$C/src dir/d.txt"
echo arc > "$C/tgt/box.sgarc"; echo plain > "$C/tgt/plain.txt"
cp "$T/probe.exe" "$C/tgt/args.exe"
printf '@echo bat[%%~1][%%~2]> C:\\filedrop-bat.txt\r\n' > "$C/tgt/show.bat"
G='{2F0E7A51-9C3B-4D16-B84A-615E70007501}'
reg() { timeout -s KILL 60 "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg "HKCR\\CLSID\\$G\\InprocServer32" /ve /d 'C:\sgdrophandler.dll'
reg "HKCR\\CLSID\\$G\\InprocServer32" /v ThreadingModel /d Apartment
reg 'HKCR\.sgarc' /ve /d 'SGArc.Archive'
reg 'HKCR\SGArc.Archive\shellex\DropHandler' /ve /d "$G"
cd "$C"
run() { timeout -s KILL 60 "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
seen() { [ -f "$C/$1" ] && tr -d '\r' < "$C/$1"; }
has() { printf '%s\n' "$out" | grep -q "$1"; }

out=$(run 'C:\tgt\box.sgarc' 'C:\src dir\a b.txt' 'C:\src dir\c.txt')
has '^target-is-folder 0$' && has '^drop 00000000 1$' \
    && seen filedrop-handler.txt | grep -qx 'drop C:\\tgt\\box.sgarc <- C:\\src dir\\a b.txt C:\\src dir\\c.txt' \
    && [ -f "$C/src dir/a b.txt" ] && [ ! -f "$C/tgt/a b.txt" ] \
    && pass "an archive's drop handler gets the files (loaded with the archive), and they stay where they were" \
    || fail "drop handler: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen filedrop-handler.txt)"

out=$(run 'C:\tgt\args.exe' 'C:\src dir\a b.txt' 'C:\src dir\c.txt')
has '^target-is-folder 0$' && has '^drop 00000000 1$' \
    && seen filedrop-args.txt | grep -qxF 'args["C:\src dir\a b.txt" "C:\src dir\c.txt"]' \
    && [ -f "$C/src dir/c.txt" ] \
    && pass "a program starts with the dropped files as its arguments (quoted when they have a space)" \
    || fail "program: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen filedrop-args.txt)"
rm -f "$C/filedrop-args.txt"

out=$(run 'C:\tgt\show.bat' 'C:\src dir\a b.txt' 'C:\src dir\c.txt')
has '^drop 00000000 1$' && seen filedrop-bat.txt | grep -qxF 'bat[C:\src dir\a b.txt][C:\src dir\c.txt]' \
    && pass "a batch file gets them as %1 %2" || fail "batch: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen filedrop-bat.txt)"

run --mklink 'C:\tgt\tool.lnk' 'C:\tgt\args.exe' '-x' >/dev/null
out=$(run 'C:\tgt\tool.lnk' 'C:\src dir\c.txt')
has '^target-is-folder 0$' && has '^drop 00000000 1$' \
    && seen filedrop-args.txt | grep -qxF 'args[-x "C:\src dir\c.txt"]' \
    && [ ! -f "$C/tgt/c.txt" ] && pass "a shortcut to a program: the program, with the shortcut's arguments and then the file" \
    || fail "shortcut to a program: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen filedrop-args.txt)"

run --mklink 'C:\tgt\into.lnk' 'C:\into' >/dev/null
out=$(run 'C:\tgt\into.lnk' 'C:\src dir\d.txt')
has '^drop 00000000' && [ -f "$C/into/d.txt" ] \
    && pass "a shortcut to a folder: the folder takes the file in" \
    || fail "shortcut to a folder: $(printf '%s\n' "$out" | tr '\n' ' '); into: $(ls "$C/into")"

python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1], "w").writestr("inside.txt", "in")' "$C/tgt/pack.zip"
out=$(run 'C:\tgt\pack.zip' 'C:\src dir\a b.txt')
has '^target-is-folder 0$' && has '^drop 00000000' \
    && python3 -I -c 'import sys, zipfile; n = zipfile.ZipFile(sys.argv[1]).namelist(); sys.exit(0 if "a b.txt" in n and "inside.txt" in n else 1)' "$C/tgt/pack.zip" \
    && pass "a .zip (Compressed Folder) takes the file in" \
    || fail "zip: $(printf '%s\n' "$out" | tr '\n' ' ') zip has: $(python3 -I -c 'import sys, zipfile; print(zipfile.ZipFile(sys.argv[1]).namelist())' "$C/tgt/pack.zip" 2>&1)"

out=$(run 'C:\tgt\plain.txt' 'C:\src dir\c.txt')
has '^target-is-folder 1$' && pass "a file with no handler leaves the drop to its folder, as before" \
    || fail "plain file: $(printf '%s\n' "$out" | tr '\n' ' ')"

# the desktop's own items: a program on the desktop
D=$(ls -d "$C"/users/*/Desktop 2>/dev/null | grep -v '/Public/' | head -1)
if [ -n "$D" ]; then
    cp "$T/probe.exe" "$D/deskargs.exe"
    rm -f "$C/filedrop-args.txt"
    out=$(run 'desktop:deskargs.exe' 'C:\src dir\a b.txt')
    seen filedrop-args.txt | grep -qxF 'args["C:\src dir\a b.txt"]' \
        && pass "a program on the desktop takes a drop too" || fail "desktop program: $(printf '%s\n' "$out" | tr '\n' ' ') saw: $(seen filedrop-args.txt)"
else
    fail "no Desktop folder in the prefix"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
