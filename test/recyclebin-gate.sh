#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Recycle Bin as Windows 10 has it (patches/sg/9065, GitHub #20 #21 #22).
#  - The bin's menu (the desktop icon's): Empty Recycle Bin, greyed while the
#    bin is empty; no Cut, Copy or Delete. Its folder's background menu has it
#    too. SHEmptyRecycleBin asks "Are you sure you want to permanently delete
#    these N items?" (No keeps them), empties folders too (DeleteFileW alone
#    left every deleted folder in the bin), and per drive.
#  - The bin's icon is the empty or the full one (it was always full), and the
#    desktop reads it again when the bin fills or empties.
#  - Properties: the bin's sheet (its locations, "Custom size: Maximum size
#    (MB)", "Don't move files to the Recycle Bin", "Display delete confirmation
#    dialog"), kept where Windows keeps them (HKCU\...\BitBucket\Volume\{GUID}
#    MaxCapacity / NukeOnDelete, ShellState's fNoConfirmRecycle) and honoured:
#    the oldest items make room, a bigger item is "too big to recycle", a
#    drive set not to recycle deletes for good, the confirmation is asked.
#    This PC > Properties opens Settings > About (ms-settings:about); the
#    user's folder its folder sheet.
#  - A drive's own Trash (.Trash-$uid, where a Linux program puts what it
#    deletes there) is part of the bin: counted, shown, emptied.
#  - #20: a program typed in File Explorer's address bar starts in the folder shown.
#
#   WINE=/opt/wine-sg/bin/wine test/recyclebin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-recyclebin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
XP=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/rbprobe.exe" "$HERE/recyclebin-probe.c" -lshell32 -lole32 -luuid -luser32 -lcomctl32 \
    || { fail "probe did not build"; exit 1; }
# a display of its own (never the person's :0; RB_DISPLAY picks the number)
N=${RB_DISPLAY:-}
if [ -n "$N" ]; then
    Xvfb ":$N" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$N" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    export DISPLAY=":$N"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    export DISPLAY=":$(cat "$T/display")"
fi
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
cp "$T/rbprobe.exe" "$WINEPREFIX/drive_c/rbprobe.exe"
C="$WINEPREFIX/drive_c"
probe() { timeout -s KILL 60 "$WINE" 'C:\rbprobe.exe' "$@" 2>/dev/null | tr -d '\r'; }
field() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
items() { field "$(probe query "$@" | grep '^query')" items; }
TRASH="$XDG_DATA_HOME/Trash"
mkfile() { head -c "$2" /dev/zero > "$1"; }

# ---- empty: the menu's command greyed, the empty icon ----------------------
out=$(probe menu); echo "      $out"
[ "$(field "$out" empty)" = grey ] && pass "an empty bin's menu has Empty Recycle Bin, greyed" \
    || fail "empty bin's menu: '$out'"
out=$(probe icon); echo "      $out"
[ "$out" = "icon -32 res=1" ] && pass "an empty bin has the empty icon (shell32,-32, which is there)" || fail "empty bin's icon: '$out' (want -32)"

# ---- a file and a folder go in ----------------------------------------------
mkfile "$C/one.txt" 1000
mkdir -p "$C/folder1/sub"; mkfile "$C/folder1/a.bin" 3000; mkfile "$C/folder1/sub/b.bin" 5000
probe trash 'C:\one.txt' >/dev/null; probe trash 'C:\folder1' >/dev/null
out=$(probe query | grep '^query'); echo "      $out"
[ "$(field "$out" items)" = 2 ] && pass "two items in the bin" || fail "after two deletes: '$out'"
[ "$(field "$out" size)" = 9000 ] && pass "the bin's size counts what a deleted folder holds (9000 bytes)" \
    || fail "the bin's size: '$out' (want 9000: the folder's 8000 counted)"
out=$(probe menu); echo "      $out"
[ "$(field "$out" empty)" = enabled ] && pass "with items, Empty Recycle Bin is on" || fail "full bin's menu: '$out'"
[ "$(field "$out" cut)$(field "$out" copy)$(field "$out" delete)" = absentabsentabsent ] \
    && pass "the bin's menu has no Cut, Copy or Delete (as Windows)" || fail "the bin's menu: '$out'"
[ "$(field "$out" properties)" = enabled ] && pass "the bin's menu has Properties" || fail "no Properties: '$out'"
[ "$(field "$out" open)" = enabled ] && [ "$(field "$out" default)" = 0 ] && pass "the bin's menu starts with Open, its default" \
    || fail "no Open first: '$out'"
out=$(probe icon); echo "      $out"
[ "$out" = "icon -33 res=1" ] && pass "a bin with items has the full icon" || fail "full bin's icon: '$out' (want -33)"
out=$(probe bgmenu); echo "      $out"
[ "$(field "$out" empty)" = enabled ] && pass "the bin folder's background menu has Empty Recycle Bin" \
    || fail "the bin's background menu: '$out'"
[ "$(field "$out" new)" = absent ] && [ "$(field "$out" paste)" = grey ] \
    && pass "the bin's background menu has no New and Paste greyed (nothing is made in the bin)" || fail "bin background: '$out'"

# ---- Empty Recycle Bin asks; No keeps; Yes empties folders too --------------
out=$(probe emptyask no); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'Delete Multiple Items: .*Are you sure you want to permanently delete these 2 items?' \
    && pass "Empty Recycle Bin asks as Windows: 'permanently delete these 2 items?'" || fail "the question: $out"
[ "$(items)" = 2 ] && pass "No keeps the items" || fail "No emptied the bin"
out=$(probe emptyask yes); echo "$out" | sed 's/^/      /'
[ "$(items)" = 0 ] && pass "Yes empties the bin" || fail "Yes: $(items) items left"
left=$(find "$TRASH/files" -mindepth 1 2>/dev/null | wc -l); infos=$(find "$TRASH/info" -mindepth 1 2>/dev/null | wc -l)
[ "$left" = 0 ] && [ "$infos" = 0 ] && pass "the deleted folder went too (Trash files and info empty)" \
    || fail "left in the Trash: $(find "$TRASH" -mindepth 2 2>/dev/null | head -5 | tr '\n' ' ')"
out=$(probe emptyask yes); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'asked: (none)' && pass "an empty bin is emptied without a question" || fail "empty bin asked: $out"

# ---- the menu's command, from the desktop icon's menu -----------------------
mkfile "$C/two.txt" 10; probe trash 'C:\two.txt' >/dev/null
out=$(probe verb empty '::{645FF040-5081-101B-9F08-00AA002F954E}' yes); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'Delete File: .*permanently delete this file' && [ "$(items)" = 0 ] \
    && pass "the icon's Empty Recycle Bin: 'Delete File ... permanently delete this file?', then empty" \
    || fail "the icon's Empty Recycle Bin: $out"

# ---- Properties: the bin's sheet --------------------------------------------
out=$(probe props-bin); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q '^sheet' && pass "the bin's Properties open its sheet ('Recycle Bin Properties')" \
    || fail "no Recycle Bin Properties: $out"
echo "$out" | grep -q '^location .*(C:) | ' && pass "its locations list the drives, with their size" \
    || fail "locations: $(echo "$out" | grep location)"
st=$(echo "$out" | grep '^maxsize')
max=$(field "$st" maxsize)
[ -n "$max" ] && [ "$max" -gt 0 ] 2>/dev/null && [ "$(field "$st" custom)" = 1 ] && [ "$(field "$st" nuke)" = 0 ] \
    && pass "the default: Custom size, $max MB (Windows' default share of the drive)" || fail "defaults: '$st'"
[ "$(field "$st" confirm)" = 0 ] && pass "Display delete confirmation dialog is off by default (Windows 10)" \
    || fail "confirmation default: '$st'"
out=$(probe props-bin 1 1 1); echo "$out" | grep -q applied || fail "could not set the sheet: $out"
reg=$("$WINE" reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\BitBucket' /s 2>/dev/null | tr -d '\r')
echo "$reg" | grep -q 'MaxCapacity.*REG_DWORD.*0x1$' && echo "$reg" | grep -q 'NukeOnDelete.*REG_DWORD.*0x1$' \
    && pass "OK writes BitBucket\\Volume\\...: MaxCapacity 1, NukeOnDelete 1" || fail "registry after OK: $reg"
echo "$reg" | grep -q 'Volume\\{' && pass "kept per volume, by its GUID, as Windows" || echo "      (no volume GUID here: $(echo "$reg" | grep Volume | head -1))"
ss=$("$WINE" reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer' /v ShellState 2>/dev/null | tr -d '\r' | grep ShellState)
echo "      $ss"
out=$(probe props-bin); st=$(echo "$out" | grep '^maxsize'); echo "      $st"
[ "$(field "$st" nuke)" = 1 ] && [ "$(field "$st" confirm)" = 1 ] && [ "$(field "$st" editenabled)" = 0 ] \
    && pass "the sheet shows what was set (don't move, confirmation on, size greyed)" || fail "reopened: '$st'"

# ---- the settings work -----------------------------------------------------
mkfile "$C/nuke.txt" 10
out=$(probe trashask 'C:\nuke.txt' no); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'asked: .*Are you sure you want to delete' && [ -f "$C/nuke.txt" ] \
    && pass "'Don't move files to the Recycle Bin': Delete asks to delete for good (No keeps it)" || fail "nuke + No: $out"
probe trash 'C:\nuke.txt' >/dev/null
[ ! -f "$C/nuke.txt" ] && [ "$(items)" = 0 ] && pass "... and deletes it for good, nothing in the bin" \
    || fail "nuke: file there=$([ -f "$C/nuke.txt" ] && echo 1 || echo 0), bin=$(items)"
probe props-bin 1 0 1 >/dev/null
mkfile "$C/ask.txt" 10
out=$(probe trashask 'C:\ask.txt' yes); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q "asked: .*send .*to the Recycle Bin" && [ "$(items)" = 1 ] \
    && pass "'Display delete confirmation dialog' on: a move to the bin asks first" || fail "confirmation on: $out"
probe props-bin 1 0 0 >/dev/null
mkfile "$C/noask.txt" 10
out=$(probe trashask 'C:\noask.txt' no); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'asked: (none)' && [ ! -f "$C/noask.txt" ] && pass "confirmation off: no question, in the bin" \
    || fail "confirmation off: $out"
probe empty >/dev/null
# 1 MB at most: the oldest makes room; bigger than that is too big to recycle
mkfile "$C/old.bin" 600000; mkfile "$C/new.bin" 600000; mkfile "$C/big.bin" 2000000; mkfile "$C/big2.bin" 2000000
probe trash 'C:\old.bin' >/dev/null; sleep 1.1; probe trash 'C:\new.bin' >/dev/null
names=$(ls "$TRASH/files" 2>/dev/null | tr '\n' ' ')
echo "      in the Trash: $names"
case " $names" in *" new.bin "*) case " $names" in *" old.bin "*) fail "over the maximum size, the oldest stayed ($names)";; \
    *) pass "over the bin's maximum size the oldest item went for good, the new one is in the bin";; esac;; \
    *) fail "the new item is not in the bin ($names)";; esac
probe trash 'C:\big.bin' >/dev/null
[ ! -f "$C/big.bin" ] && ! ls "$TRASH/files" | grep -q '^big.bin' && pass "bigger than the bin, unasked: deleted for good" \
    || fail "too big, unasked: there=$([ -f "$C/big.bin" ] && echo 1 || echo 0), trash: $(ls "$TRASH/files" | tr '\n' ' ')"
out=$(probe trashask 'C:\big2.bin' no); echo "$out" | sed 's/^/      /'
echo "$out" | grep -q 'File Too Large for Recycle Bin: .*too big to recycle' && [ -f "$C/big2.bin" ] \
    && pass "bigger than the bin: 'too big to recycle. Permanently delete it?' (No keeps it)" || fail "too big, asked: $out"
probe props-bin 4096 0 0 >/dev/null
probe empty >/dev/null

# ---- a drive's own Trash ---------------------------------------------------
UID_=$(id -u)
mkdir -p "$T/drv/.Trash-$UID_/files" "$T/drv/.Trash-$UID_/info"
mkfile "$T/drv/.Trash-$UID_/files/linux.txt" 4321
printf '[Trash Info]\nPath=docs/linux.txt\nDeletionDate=2026-11-20T10:00:00\n' > "$T/drv/.Trash-$UID_/info/linux.txt.trashinfo"
mkdir -p "$T/drv/docs"
ln -sfn "$T/drv" "$WINEPREFIX/dosdevices/d:"
"$WINE" reg add 'HKLM\Software\Wine\Drives' /v 'd:' /d hd /f >/dev/null 2>&1
"$WINESERVER" -w
out=$(probe query 'D:\' | grep '^query'); echo "      $out"
[ "$(field "$out" items)" = 1 ] && [ "$(field "$out" size)" = 4321 ] \
    && pass "a drive's own Trash (.Trash-$UID_) is that drive's bin" || fail "D:'s bin: '$out'"
[ "$(items 'C:\')" = 0 ] && pass "C:'s bin does not count D:'s" || fail "C:'s bin counts $(items 'C:\')"
out=$(probe icon); echo "      $out"
[ "$out" = "icon -33 res=1" ] && pass "the bin's icon is full for a drive's Trash too" || fail "icon with D:'s Trash: $out"
probe empty 'D:\' >/dev/null
[ ! -e "$T/drv/.Trash-$UID_/files/linux.txt" ] && [ ! -e "$T/drv/.Trash-$UID_/info/linux.txt.trashinfo" ] \
    && pass "emptying D:'s bin empties its Trash" || fail "D:'s Trash after Empty: $(ls -R "$T/drv/.Trash-$UID_")"
rm -f "$WINEPREFIX/dosdevices/d:"

# ---- This PC and the user's folder ------------------------------------------
"$WINE" reg add 'HKCU\Software\Classes\ms-settings' /v 'URL Protocol' /d '' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Classes\ms-settings\shell\open\command' /ve \
    /d 'C:\windows\system32\cmd.exe /c echo %1>>C:\settings.txt' /f >/dev/null 2>&1
"$WINESERVER" -w
probe props-item '::{20D04FE0-3AEA-1069-A2D8-08002B30309D}' >/dev/null
for i in 1 2 3 4 5 6 7 8 9 10; do [ -s "$C/settings.txt" ] && break; sleep 0.5; done
got=$(tr -d '\r ' < "$C/settings.txt" 2>/dev/null)
[ "$got" = "ms-settings:about" ] && pass "This PC > Properties opens Settings > About (ms-settings:about)" \
    || fail "This PC > Properties: '$got'"
out=$(probe props-item '::{59031a47-3f72-44a7-89c5-5595fe6b30ee}'); echo "      $out"
echo "$out" | grep -qx "sheet $(id -un) Properties | $(id -un)" \
    && pass "the user's folder > Properties opens the folder's sheet" || fail "user folder > Properties: '$out'"

# ---- the desktop's bin icon follows the bin ---------------------------------
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
probe empty >/dev/null
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
dbin() { tr -d '\r' < "$C/desk.txt" 2>/dev/null | head -1 | tr ' ' '\n' | sed -n 's/^bin=//p'; }
wait_bin() { for i in $(seq 1 60); do [ "$(dbin)" = "$1" ] && return 0; sleep 0.25; done; return 1; }
SG_DESKTOP_DUMP='C:\desk.txt' "$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 5
"$WINE" 'C:\rbprobe.exe' idle 60 >/dev/null 2>&1 &
wait_bin 0 && pass "the desktop's bin starts empty" || fail "desktop bin state at start: '$(dbin)'"
mkfile "$C/desk1.txt" 10
probe trash 'C:\desk1.txt' >/dev/null
wait_bin 1 && pass "a delete elsewhere: the desktop's bin icon becomes the full one" \
    || fail "desktop bin after a delete: '$(dbin)'"
# a Linux program empties the Trash: the icon follows too
rm -rf "$TRASH/files/"* "$TRASH/info/"*
wait_bin 0 && pass "the Trash emptied outside Windows programs: the icon is the empty one again" \
    || fail "desktop bin after rm: '$(dbin)'"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
"$WINE" reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1
"$WINESERVER" -w

# ---- the user's folder opens at C:\users\<name> ------------------------------
"$WINE" explorer.exe '::{59031a47-3f72-44a7-89c5-5595fe6b30ee}' >/dev/null 2>&1 &
out=$(probe title); echo "      $out"
[ "$out" = "title $(id -un)" ] && pass "the user's folder opens at C:\\users\\$(id -un) (it was an empty page)" \
    || fail "the user's folder opened as: '$out'"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null

# ---- #20: the address bar starts a program in the folder shown -------------
mkdir -p "$C/users/Public/Documents/work"
cp "$T/rbprobe.exe" "$C/windows/rbprobe.exe"
"$WINE" explorer.exe 'C:\users\Public\Documents\work' >/dev/null 2>&1 &
W="$C/users/Public/Documents/work"
# a program on the path with arguments ("notepad file.txt"), then one by its full path
out=$(probe addr 'rbprobe cwd-report here.txt'); echo "      $out"
for i in $(seq 1 40); do [ -s "$W/here.txt" ] && break; sleep 0.25; done
got=$(tr -d '\r' < "$W/here.txt" 2>/dev/null)
[ "$got" = 'C:\users\Public\Documents\work' ] \
    && pass "'rbprobe cwd-report here.txt' typed in the address bar: runs, in the folder shown (its file lands there)" \
    || fail "address bar, program on the path: '$got' ($(find "$C" -name here.txt 2>/dev/null))"
out=$(probe addr 'C:\rbprobe.exe cwd-report here2.txt'); echo "      $out"
for i in $(seq 1 40); do [ -s "$W/here2.txt" ] && break; sleep 0.25; done
got=$(tr -d '\r' < "$W/here2.txt" 2>/dev/null)
[ "$got" = 'C:\users\Public\Documents\work' ] \
    && pass "a program by its full path with arguments: runs in the folder shown" \
    || fail "address bar, full path: '$got' ($(find "$C" -name here2.txt 2>/dev/null))"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
