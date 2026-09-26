#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer, round 2 (patches/sg/0150-0156), driven like a person on X, in a shell session.
#
# Pictures shows its pictures (a PNG's own pixels in the view, at Large and
# Extra large icons) and a selected picture is not tinted; a file dragged onto
# the navigation pane's Pictures moves there; the Tiles, Content and Extra
# large views; Group by Type puts headers in the view; the preview pane shows a
# text file's content and the details pane opens; a file opened from the view
# is a recent file; a folder visited twice is frequent; "Pin to Quick access"
# shows in the navigation pane and on the Quick access page; the type column
# reads "Text Document", "PNG File", "Application", "Compressed (zipped)
# Folder"; Send to > Compressed (zipped) Folder makes a valid zip (with
# sg-shell's sg-zip, SGZIP=) and Send to > Desktop (create shortcut) a shortcut.
#
#   WINE=/opt/wine-sg/bin/wine test/explorer2-gate.sh    (ARTIFACTS=DIR keeps screenshots and logs)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
SGZIP="${SGZIP:-$HERE/../../sg-shell/build/sg-zip64.exe}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
skip() { printf 'SKIP  %s\n' "$*"; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-explorer2.XXXXXX)
# a home of its own: the prefix's user folders must not be the developer's
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/explorer-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1

# Send to: the items sg-session plants in a profile, and sg-zip's drop command
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
UD="$WINEPREFIX/drive_c/users/$U"
SENDTO="$UD/AppData/Roaming/Microsoft/Windows/SendTo"
mkdir -p "$SENDTO"
: > "$SENDTO/Compressed (zipped) Folder.ZFSendToTarget"
: > "$SENDTO/Desktop (create shortcut).DeskLink"
: > "$SENDTO/Documents.mydocs"
HAVE_ZIP=0
if [ -f "$SGZIP" ]; then
    cp "$SGZIP" "$WINEPREFIX/drive_c/sg-zip64.exe"
    "$WINE" reg add 'HKCR\.ZFSendToTarget' /ve /d CompressedFolderSendTarget /f >/dev/null 2>&1
    "$WINE" reg add 'HKCR\CompressedFolderSendTarget\shell\sendto\command' /ve /d '"C:\sg-zip64.exe" /create %*' /f >/dev/null 2>&1
    HAVE_ZIP=1
fi
"$WINESERVER" -w

# the user's folders, real directories with something in them
for d in Desktop Documents Downloads Music Pictures Videos; do [ -L "$UD/$d" ] && rm -f "$UD/$d"; mkdir -p "$UD/$d"; done
DOC="$UD/Documents"
PIC="$UD/Pictures"
mkdir -p "$DOC/Projects" "$DOC/deep" "$DOC/DragSrc"
printf 'hello preview\r\nsecond line\r\n' > "$DOC/notes.txt"
echo report > "$DOC/report.txt"
echo unicode > "$DOC/日本語.txt"
echo drag > "$DOC/DragSrc/dragme.txt"
echo data > "$DOC/data.bin"
cp "$WINEPREFIX/drive_c/windows/system32/notepad.exe" "$DOC/tool.exe"
python3 - "$PIC" "$DOC" <<'PYEOF'
import sys, zipfile
from PIL import Image
Image.new('RGB', (400, 300), (0x20, 0xc0, 0x40)).save(sys.argv[1] + '/sample.png')
Image.new('RGB', (300, 400), (0xc0, 0x30, 0x20)).save(sys.argv[1] + '/portrait.jpg', quality=95)
Image.new('RGB', (64, 64), (0x20, 0xc0, 0x40)).save(sys.argv[2] + '/small.png')
with zipfile.ZipFile(sys.argv[2] + '/archive.zip', 'w') as z: z.writestr('a.txt', 'a')
# a photo stored on its side: 400x200, red left, blue right, EXIF orientation 6 (turn it right)
im = Image.new('RGB', (400, 200), (0xe0, 0x10, 0x10))
im.paste((0x10, 0x10, 0xe0), (200, 0, 400, 200))
exif = Image.Exif(); exif[0x0112] = 6
im.save(sys.argv[2] + '/sideways.jpg', quality=95, exif=exif)
PYEOF
WDOC="C:\\users\\$U\\Documents"
WPIC="C:\\users\\$U\\Pictures"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
export SG_ZIP_NO_SHOW=1
P() { "$WINE" explorer-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
shot() { sleep 1; import -window root "$T/\$1.png"; }
at() { set -- \$(P origin | sed 's/origin=//; s/,/ /'); ox=\$1; oy=\$2; }
click() { at; xdotool mousemove \$((ox + \$1)) \$((oy + \$2)) click \${3:-1}; }
go() { at; xdotool mousemove \$((ox + 300)) \$((oy + 575)) click 1; sleep 0.3; xdotool key ctrl+l; sleep 0.5; xdotool key ctrl+a; xdotool type --delay 20 "\$1"; xdotool key Return; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/desktop.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/desktop.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2

# Pictures: Large icons with the pictures themselves
WINEDEBUG=err+all,trace+explorer "$WINE" explorer "$WPIC" > "$T/explorer.out" 2>&1 &
echo "pictures: \$(P wait-title Pictures 20)" >> "$T/log.out"
sleep 4; shot thumbs-large

# View > Extra large icons (the first entry), then select the picture
click 458 25; sleep 1; xdotool key Down Return; sleep 4; shot thumbs-xl
"$WINE" explorer "/select,$WPIC\\\\sample.png" >/dev/null 2>&1 &
sleep 3; shot thumbs-selected

# a file dragged onto the navigation pane's This PC > Pictures (the eleventh row) moves there
go '$WDOC\\DragSrc'
echo "dragsrc: \$(P wait-title DragSrc 10)" >> "$T/log.out"
sleep 1; click 600 400; sleep 0.5
at; xdotool mousemove \$((ox + 270)) \$((oy + 119)); sleep 0.3; xdotool mousedown 1; sleep 0.3
for step in 1 2 3 4 5 6 7 8; do xdotool mousemove_relative -- -20 30; sleep 0.15; done
at; xdotool mousemove \$((ox + 100)) \$((oy + 92 + 10 * 26 + 13)); sleep 0.5
xdotool mousemove_relative 3 1; sleep 0.5; shot dragging; xdotool mouseup 1; sleep 3

# Documents in Details: Sort > Group by > Type
go '$WDOC'
echo "documents: \$(P wait-title Documents 10)" >> "$T/log.out"
sleep 1; click 380 25; sleep 1; xdotool key Up Right; sleep 0.5; xdotool key Down Down Return; sleep 2; shot grouped

# View > Tiles, View > Content (and back to Details): pictures for a person to look at
click 458 25; sleep 1; xdotool key Down Down Down Down Down Down Down Return; sleep 3; shot tiles
click 458 25; sleep 1; xdotool key Down Down Down Down Down Down Down Down Return; sleep 3; shot content
click 458 25; sleep 1; xdotool key Down Down Down Down Down Down Return; sleep 2

# the preview pane, then the details pane
"$WINE" explorer "/select,$WDOC\\\\notes.txt" >/dev/null 2>&1 &
sleep 3; xdotool key alt+p; sleep 2
P panetext > "$T/preview.out"
shot preview
xdotool key alt+shift+p; sleep 2
P findchild SGExplorerPane > "$T/details.out"
shot details
xdotool key alt+shift+p; sleep 1

# Send to > Compressed (zipped) Folder, then Send to > Desktop (create shortcut)
"$WINE" explorer "/select,$WDOC\\\\report.txt" >/dev/null 2>&1 &
sleep 3; xdotool key shift+F10; sleep 1; shot sendto-menu; xdotool key n; sleep 1; shot sendto; xdotool key Return; sleep 4
"$WINE" explorer "/select,$WDOC\\\\report.txt" >/dev/null 2>&1 &
sleep 3; xdotool key shift+F10; sleep 1; xdotool key n; sleep 1; xdotool key Down Return; sleep 3

# a file opened from the view is a recent file
"$WINE" explorer "/select,$WDOC\\\\notes.txt" >/dev/null 2>&1 &
sleep 3; xdotool key Return; sleep 4
"$WINE" taskkill /f /im notepad.exe >/dev/null 2>&1

# a folder visited twice is frequent
go '$WDOC\\deep'; P wait-title deep 10 >/dev/null; sleep 1
go '$WDOC'; P wait-title Documents 10 >/dev/null; sleep 1
go '$WDOC\\deep'; P wait-title deep 10 >/dev/null; sleep 2

# Pin to Quick access (the folder's context menu verb), then the Quick access page
P verb '$WDOC\\Projects' pintohome > "$T/pin.out"
sleep 3
click 60 105
echo "quick: \$(P wait-title 'Quick access' 10)" >> "$T/log.out"
sleep 2; shot quick

# Unpin from Quick access
P verb '$WDOC\\Projects' unpinfromhome >> "$T/pin.out"
sleep 3

P types '$WDOC\\notes.txt' '$WPIC\\sample.png' '$WDOC\\tool.exe' '$WDOC\\archive.zip' '$WDOC\\Projects' > "$T/types.out"
P groups '$WDOC' 4 > "$T/groups.out"
P tiles '$WDOC' > "$T/tiles.out"
P recent '$WDOC\\日本語.txt' > "$T/recent.out"
P thumb '$WDOC\\sideways.jpg' 128 > "$T/thumb.out"
P thumb '$WDOC\\notes.txt' 128 >> "$T/thumb.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 500 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh"
cat "$T/log.out" 2>/dev/null | sed 's/^/  /'

# pixels of a colour in the view's area of a screenshot
count() {
    python3 - "$T/$1.png" "$2" <<'PYEOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
want = tuple(int(sys.argv[2][i:i + 2], 16) for i in (0, 2, 4))
w, h = im.size
print(sum(1 for x in range(230, min(w, 1000)) for y in range(120, min(h, 600))
          if all(abs(a - b) <= 2 for a, b in zip(im.getpixel((x, y)), want))))
PYEOF
}

grep -q '^pictures: title=Pictures$' "$T/log.out" && pass "File Explorer opens Pictures" || fail "no Pictures window"
n=$(count thumbs-large 20c040)
[ "${n:-0}" -gt 5000 ] && pass "Pictures shows the PNG's own pixels (Large icons, $n px)" || fail "no thumbnail at Large icons ($n px)"
n=$(count thumbs-xl 20c040)
[ "${n:-0}" -gt 40000 ] && pass "Extra large icons shows it at 256 ($n px)" || fail "no Extra large thumbnail ($n px)"
n=$(count thumbs-selected 20c040)
[ "${n:-0}" -gt 40000 ] && pass "a selected picture is not tinted ($n px of its colour)" || fail "the selected picture is tinted ($n px)"
n=$(count thumbs-xl c03020)
[ "${n:-0}" -gt 20000 ] && pass "a JPEG's thumbnail too ($n px)" || fail "no JPEG thumbnail ($n px)"

[ -f "$PIC/dragme.txt" ] && [ ! -f "$DOC/DragSrc/dragme.txt" ] && pass "a file dropped on the navigation pane's Pictures moves there" ||
    fail "drop on the navigation pane: Pictures has [$(ls "$PIC" | tr '\n' ' ')], DragSrc has [$(ls "$DOC/DragSrc" | tr '\n' ' ')]"

grep -q '^groups=[2-9]' "$T/groups.out" && grep -q '^header=Text Document (3)$' "$T/groups.out" && pass "Group by Type: groups with headers ($(head -1 "$T/groups.out"))" ||
    fail "groups: $(tr '\n' ' ' < "$T/groups.out")"
n=$(count grouped e2d7eb)
[ "${n:-0}" -gt 300 ] && pass "Sort > Group by > Type draws the group headers ($n px of their lines)" || fail "no group headers on screen ($n px)"
grep -q '^tiles view=4 lines=2 width=250' "$T/tiles.out" && pass "Tiles: tiles of 250 px with two lines (type, size)" || fail "tiles: $(sed -n 1p "$T/tiles.out")"
grep -q '^content view=4 lines=1 width=[5-9][0-9][0-9]' "$T/tiles.out" && pass "Content: rows across the view" || fail "content: $(sed -n 2p "$T/tiles.out")"
grep -q '^icons view=0 size=256$' "$T/tiles.out" && pass "Extra large icons are 256 px" || fail "icon size: $(sed -n 3p "$T/tiles.out")"

# EXIF orientation 6: the 400x200 photo shows 64x128, its left (red) half on top
python3 - "$T/thumb.out" <<'PYEOF' && pass "a photo's EXIF orientation is applied ($(head -1 "$T/thumb.out"))" || fail "orientation: $(head -1 "$T/thumb.out")"
import sys, re
m = re.match(r'thumb=(\d+)x(\d+) top=(\w+) bottom=(\w+)', open(sys.argv[1]).readline())
w, h, top, bottom = int(m[1]), int(m[2]), int(m[3], 16), int(m[4], 16)
red = lambda c: (c >> 16) > 0xb0 and (c & 0xff) < 0x50
blue = lambda c: (c & 0xff) > 0xb0 and (c >> 16) < 0x50
assert (w, h) == (64, 128) and red(top) and blue(bottom)
PYEOF
sed -n 2p "$T/thumb.out" | grep -q '^thumb hr=0x80004005$' && pass "a text file has no thumbnail" || fail "text thumbnail: $(sed -n 2p "$T/thumb.out")"
grep -q '^panetext=hello preview$' "$T/preview.out" && pass "the preview pane shows a text file's content" || fail "preview pane: $(cat "$T/preview.out")"
grep -q '^found=1$' "$T/details.out" && pass "Alt+Shift+P opens the details pane" || fail "details pane: $(cat "$T/details.out")"

if [ $HAVE_ZIP = 1 ]; then
    python3 - "$DOC/report.zip" 2>/dev/null <<'PYEOF' && pass "Send to > Compressed (zipped) Folder makes a valid zip" || fail "no valid report.zip: $(ls "$DOC" | tr '\n' ' ')"
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
assert z.testzip() is None and z.read('report.txt') == b'report\n'
PYEOF
else
    skip "Send to > Compressed (zipped) Folder (no sg-zip at $SGZIP)"
fi
ls "$UD/Desktop" | grep -q '^report - Shortcut.lnk$' && pass "Send to > Desktop (create shortcut) makes a shortcut" || fail "desktop: $(ls "$UD/Desktop" | tr '\n' ' ')"

grep -q "quick access pin: .*Projects" "$T/explorer.out" && pass "Pin to Quick access shows in the navigation pane" || fail "no pin in the navigation pane ($(cat "$T/pin.out"))"
pins=$(grep -o 'quick access: [0-9]* pinned' "$T/explorer.out" | uniq | tail -2 | tr '\n' ',')
tail -1 "$T/pin.out" | grep -q '^verb hr=0$' && [ "$pins" = "quick access: 5 pinned,quick access: 4 pinned," ] &&
    pass "Unpin from Quick access takes it away again" || fail "unpin: $(tail -1 "$T/pin.out"), $pins"
ls "$UD/AppData/Roaming/Microsoft/Windows/Recent" 2>/dev/null | grep -qx '日本語.txt.lnk' &&
    pass "a recent document named outside the code page gets its shortcut" || fail "recent: $(ls "$UD/AppData/Roaming/Microsoft/Windows/Recent" 2>&1 | tr '\n' ' ')"
grep -q '^quick: title=Quick access$' "$T/log.out" && pass "Quick access is a page of its own" || fail "no Quick access page"
grep -q 'quick access folder: L"Projects" (pinned)' "$T/explorer.out" && pass "the Quick access page lists the pinned folder" || fail "Quick access page: no Projects"
grep -q 'quick access frequent: .*deep' "$T/explorer.out" && pass "a folder visited twice is a frequent folder" || fail "no frequent folder"
grep -q 'quick access file: L"notes.txt"' "$T/explorer.out" && pass "a file opened from the view is a recent file" || fail "no recent file"

sed -n 1p "$T/types.out" | grep -qx 'type=Text Document' && pass "type: Text Document" || fail "txt type: $(sed -n 1p "$T/types.out")"
sed -n 2p "$T/types.out" | grep -qx 'type=PNG File' && pass "type: PNG File" || fail "png type: $(sed -n 2p "$T/types.out")"
sed -n 3p "$T/types.out" | grep -qx 'type=Application' && pass "type: Application" || fail "exe type: $(sed -n 3p "$T/types.out")"
sed -n 4p "$T/types.out" | grep -qx 'type=Compressed (zipped) Folder' && pass "type: Compressed (zipped) Folder" || fail "zip type: $(sed -n 4p "$T/types.out")"
sed -n 5p "$T/types.out" | grep -qx 'type=File folder' && pass "type: File folder" || fail "folder type: $(sed -n 5p "$T/types.out")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
