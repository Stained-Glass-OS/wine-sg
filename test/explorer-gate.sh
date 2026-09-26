#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer (patches/sg/0110-0112), driven like a person on X, in a shell session.
#
# Win+E's File Explorer opens on This PC (folders, and drives with capacity
# bars); the navigation pane goes to Documents; a search finds a file two
# folders down; the address bar takes a typed path and Up goes back; a click
# on a crumb goes there; Ctrl+Shift+N makes a folder that is renamed as
# typed; `explorer /select,FILE` selects the file (F2 renames it); a large
# copy shows its progress; a copy onto an existing file asks and keeps both;
# Delete sends a file to the Recycle Bin without asking; the details view's
# columns are Windows' and sort as asked.
#
#   WINE=/opt/wine-sg/bin/wine test/explorer-gate.sh     (ARTIFACTS=DIR keeps screenshots and logs)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-explorer.XXXXXX)
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
"$WINESERVER" -w

# the user's folders, real directories with something in them
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
UD="$WINEPREFIX/drive_c/users/$U"
for d in Desktop Documents Downloads Music Pictures Videos; do [ -L "$UD/$d" ] && rm -f "$UD/$d"; mkdir -p "$UD/$d"; done
DOC="$UD/Documents"
mkdir -p "$DOC/Projects" "$DOC/deep/deeper"
echo needle > "$DOC/deep/deeper/needle-in-a-haystack.txt"
echo notes > "$DOC/notes.txt"
echo mine > "$DOC/same.txt"
echo theirs > "$DOC/Projects/same.txt"
echo bye > "$DOC/trashme.txt"
dd if=/dev/urandom of="$DOC/big.bin" bs=1M count=1536 status=none
WDOC="C:\\users\\$U\\Documents"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" explorer-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
shot() { sleep 1; import -window root "$T/\$1.png"; }
at() { set -- \$(P origin | sed 's/origin=//; s/,/ /'); ox=\$1; oy=\$2; }
click() { at; xdotool mousemove \$((ox + \$1)) \$((oy + \$2)) click \${3:-1}; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/desktop.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/desktop.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2

# Win+E's File Explorer: This PC
WINEDEBUG=err+all,trace+explorer "$WINE" explorer > "$T/explorer.out" 2>&1 &
echo "thispc: \$(P wait-title 'This PC' 20)" >> "$T/log.out"
shot thispc

# the navigation pane: This PC > Documents (the eighth row)
click 100 \$((92 + 7 * 26 + 13))
echo "navpane: \$(P wait-title Documents 10)" >> "$T/log.out"
shot documents

# search: Ctrl+F, a word, Enter
xdotool key ctrl+f; sleep 0.5; xdotool type --delay 40 haystack; xdotool key Return
echo "search: \$(P wait-title 'Search Results in Documents' 10)" >> "$T/log.out"
sleep 3; shot search
xdotool key Escape
echo "search-closed: \$(P wait-title Documents 10)" >> "$T/log.out"

# the address bar: Ctrl+L, a path, Enter; then Up
xdotool key ctrl+l; sleep 0.5; xdotool key ctrl+a; xdotool type --delay 20 '$WDOC\\Projects'; xdotool key Return
echo "address: \$(P wait-title Projects 10)" >> "$T/log.out"
shot projects
# a crumb: This PC > Documents > Projects -- Documents is the second
click 262 70
echo "crumb: \$(P wait-title Documents 10)" >> "$T/log.out"

# a new folder, named as it is typed
click 600 480
xdotool key ctrl+shift+n; sleep 1.5; xdotool type --delay 40 'Holiday photos'; xdotool key Return; sleep 1
shot newfolder

# explorer /select, then F2
"$WINE" explorer "/select,$WDOC\\notes.txt" >/dev/null 2>&1 &
sleep 3; shot select
xdotool key F2; sleep 1; xdotool type --delay 40 renamed; xdotool key Return; sleep 1

# Delete: to the Recycle Bin, no questions
"$WINE" explorer "/select,$WDOC\\trashme.txt" >/dev/null 2>&1 &
sleep 3; xdotool key Delete; sleep 2

# a copy onto an existing file asks; keep both
"$WINE" explorer "/select,$WDOC\\same.txt" >/dev/null 2>&1 &
sleep 3; xdotool key ctrl+c; sleep 0.5
xdotool key ctrl+l; sleep 0.5; xdotool key ctrl+a; xdotool type --delay 20 '$WDOC\\Projects'; xdotool key Return
P wait-title Projects 10 >/dev/null
P watch SGFileConflict 15 > "$T/conflict.out" &
sleep 0.5; xdotool key ctrl+v; sleep 3; shot conflict
xdotool key k; sleep 2

# a large copy shows its progress
xdotool key alt+Up; P wait-title Documents 10 >/dev/null
"$WINE" explorer "/select,$WDOC\\big.bin" >/dev/null 2>&1 &
sleep 3; xdotool key ctrl+c; sleep 0.5
xdotool key ctrl+l; sleep 0.5; xdotool key ctrl+a; xdotool type --delay 20 '$WDOC\\Projects'; xdotool key Return
P wait-title Projects 10 >/dev/null
P watch SGFileOperation 30 > "$T/progress.out" &
sleep 0.5; xdotool key ctrl+v; sleep 1.2; shot progress
i=0; while [ \$i -lt 120 ] && [ "\$(stat -c %s '$DOC/Projects/big.bin' 2>/dev/null)" != "\$(stat -c %s '$DOC/big.bin')" ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2; shot copied
P columns '$WDOC' > "$T/columns.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh"
cat "$T/log.out" 2>/dev/null | sed 's/^/  /'

grep -q '^thispc: title=This PC$' "$T/log.out" && pass "File Explorer opens on This PC" || fail "no This PC window"
# This PC's capacity bars: the accent purple in the content area, under the folders
bars=$(python3 - "$T/thispc.png" <<'PYEOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
w, h = im.size
print(sum(1 for x in range(230, min(w, 1000)) for y in range(300, min(h, 600)) if im.getpixel((x, y)) == (0x7b, 0x2f, 0xbe)))
PYEOF
)
[ "${bars:-0}" -gt 200 ] && pass "This PC shows drives with capacity bars" || fail "no capacity bars ($bars accent pixels)"
grep -q '^navpane: title=Documents$' "$T/log.out" && pass "the navigation pane goes to This PC > Documents" || fail "navigation pane"
grep -q '^search: title=Search Results in Documents$' "$T/log.out" && pass "a search shows its results" || fail "no search results"
grep -q 'search done, [1-9]' "$T/explorer.out" && pass "the search found the file two folders down" || fail "search found nothing"
grep -q '^search-closed: title=Documents$' "$T/log.out" && pass "Escape goes back to the folder" || fail "search did not close"
grep -q '^address: title=Projects$' "$T/log.out" && pass "a typed path in the address bar is followed" || fail "address bar"
grep -q '^crumb: title=Documents$' "$T/log.out" && pass "a click on a crumb goes there" || fail "breadcrumb"
[ -d "$DOC/Holiday photos" ] && pass "Ctrl+Shift+N makes a folder, renamed as typed" || fail "new folder: $(ls "$DOC" | tr '\n' ' ')"
[ -f "$DOC/renamed.txt" ] && [ ! -f "$DOC/notes.txt" ] && pass "explorer /select, selects the file (F2 renamed it)" || fail "/select + F2: $(ls "$DOC" | tr '\n' ' ')"
[ ! -e "$DOC/trashme.txt" ] && ls "$HOME/.local/share/Trash/files" 2>/dev/null | grep -q trashme && pass "Delete sends the file to the Recycle Bin" || fail "delete: $(ls "$HOME/.local/share/Trash/files" 2>&1 | tr '\n' ' ')"
grep -q 'seen=1' "$T/conflict.out" 2>/dev/null && pass "a copy onto an existing file asks (Replace or Skip Files)" || fail "no conflict dialog"
[ -f "$DOC/Projects/same (2).txt" ] && [ "$(cat "$DOC/Projects/same.txt")" = theirs ] && pass "Keep both keeps both" || fail "keep both: $(ls "$DOC/Projects" | tr '\n' ' ')"
grep -q 'seen=1' "$T/progress.out" 2>/dev/null && pass "a large copy shows its progress" || fail "no progress window"
cmp -s "$DOC/big.bin" "$DOC/Projects/big.bin" && pass "the copy completes, byte for byte" || fail "copy incomplete"
grep -q '^columns=Name|Date modified|Type|Size$' "$T/columns.out" && pass "details view: Name, Date modified, Type, Size" || fail "columns: $(head -1 "$T/columns.out")"
grep -q '^sort hr=0 pid=12 direction=-1$' "$T/columns.out" && pass "the view sorts as asked (Size, descending)" || fail "sort: $(sed -n 2p "$T/columns.out")"
grep -q '^selected=1$' "$T/columns.out" && pass "the view counts its selection" || fail "selection count: $(sed -n 3p "$T/columns.out")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
