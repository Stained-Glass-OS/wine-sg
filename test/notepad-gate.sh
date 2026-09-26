#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Notepad is Stained Glass's editor (patches/sg/0100), for everything that
# runs notepad.exe -- in a shell session, under xvfb.
#
#  - `notepad file` from cmd, a 64-bit and a 32-bit program's CreateProcess of
#    "notepad.exe", and ShellExecute of a .txt all start it (system32 and
#    syswow64 are both ours);
#  - UTF-8 (no BOM, CRLF, astral characters), UTF-16 LE (BOM, LF) and UTF-16 BE
#    (BOM, CR) files show the right text and the right encoding / line ends,
#    and an edit-then-revert save writes back the very same bytes;
#  - find, and replace all, plain and with a regular expression and groups;
#  - tabs: open with Ctrl+O, several files on the command line, Ctrl+N, Ctrl+W,
#    Ctrl+Tab;
#  - syntax highlighting: the control's own answer for a keyword, a comment and
#    a string, and the keyword's pixels on the screen are purple where an
#    identifier's are not.
#
#   WINE=/opt/wine-sg/bin/wine test/notepad-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree: obj/server/wineserver)
#   ARTIFACTS=DIR keeps screenshots and logs
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: needs both mingw compilers"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-notepad.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/notepad-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/notepad-probe.c" || { fail "32-bit probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$T/probe32.exe" "$C/"
mkdir -p "$C/t"
# the fixtures: bytes chosen to catch a decoder or encoder that is nearly right
printf 'h\303\251llo w\303\266rld \342\234\223 \346\227\245\346\234\254 \360\237\230\200\r\nline 2 has 42 and 7\r\n' > "$C/t/utf8.txt"
printf 'first\nsecond \342\202\254 123\nthird\n' | iconv -f UTF-8 -t UTF-16LE | { printf '\377\376'; cat; } > "$C/t/utf16.txt"
printf 'alpha\rbeta \316\261\316\262\316\263\rgamma' | iconv -f UTF-8 -t UTF-16BE | { printf '\376\377'; cat; } > "$C/t/be.txt"
printf 'the cat sat on the mat\r\nline 12 and 345\r\n' > "$C/t/find.txt"
printf '#include <stdio.h>\r\n/* note */\r\nint main(void)\r\n{\r\n    int count = 3;\r\n    printf("%%d\\n", count);\r\n    return count;\r\n}\r\n' > "$C/t/sample.c"
printf 'opened by association\r\n' > "$C/t/assoc.txt"
for f in utf8 utf16 be find sample; do cp "$C/t/$f."* "$T/orig-$f" 2>/dev/null; done
cp "$C/t/sample.c" "$T/orig-sample"

"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$C"
# probe output goes to a file: a program it starts would hold a pipe open
P() { printf '%s\n' "\$*" >> "$T/log.out"; "$WINE" probe64.exe "\$@" > "$T/p.out" 2>/dev/null; tr -d '\r' < "$T/p.out" >> "$T/log.out"; echo >> "$T/log.out"; }
P32() { printf '32 %s\n' "\$*" >> "$T/log.out"; "$WINE" probe32.exe "\$@" > "$T/p.out" 2>/dev/null; tr -d '\r' < "$T/p.out" >> "$T/log.out"; echo >> "$T/log.out"; }
focus() { w=\$(xdotool search --name 'Notepad\$' | head -1); [ -n "\$w" ] && xdotool windowfocus "\$w"; sleep 0.4; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2

# 1. from cmd, by name
"$WINE" cmd /c notepad.exe 'C:\\t\\utf8.txt' > /dev/null 2>&1 &
P wait; sleep 1
P class; P tabs; P enc; P dump 0 'C:\\t\\dump-utf8'
focus; xdotool type x; xdotool key BackSpace; P modified
xdotool key ctrl+s; sleep 1; P modified

# 2. Ctrl+O: a second tab
focus; xdotool key ctrl+o; sleep 2; xdotool type --delay 15 'C:\\t\\utf16.txt'; xdotool key Return; sleep 2
P tabs; P enc; P dump 1 'C:\\t\\dump-utf16'
focus; xdotool key ctrl+End; xdotool type x; xdotool key BackSpace; xdotool key ctrl+s; sleep 1; P modified

# 3. find and replace
focus; xdotool key ctrl+o; sleep 2; xdotool type --delay 15 'C:\\t\\find.txt'; xdotool key Return; sleep 2
P tabs
focus; xdotool key ctrl+Home; xdotool key ctrl+f; sleep 0.5; xdotool type --delay 20 'sat'; sleep 0.5; xdotool key Escape; sleep 0.3
P getsel
focus; xdotool key ctrl+h; sleep 0.5
P findbar 'the' 'a'; P click 1032; sleep 0.5; P dump 2 'C:\\t\\dump-plain'
P click 1028; P findbar '(\\d+)' '<\$1>'; P click 1032; sleep 0.5; P dump 2 'C:\\t\\dump-regex'
P click 1028
focus; xdotool key Escape

# 4. highlighting
focus; xdotool key ctrl+o; sleep 2; xdotool type --delay 15 'C:\\t\\sample.c'; xdotool key Return; sleep 2
P tabs; P lang
P style 55; P style 12; P style 103; P style 59; P style 22
focus; xdotool key ctrl+End; sleep 1
P screenpos 103; P screenpos 59
import -window root "$T/highlight.png"

# 5. tabs
focus; xdotool key ctrl+n; sleep 0.5; P tabs
focus; xdotool key ctrl+Tab; sleep 0.3; P tabs
focus; xdotool key ctrl+w; sleep 0.5; P tabs
focus; xdotool key ctrl+w; sleep 0.5; P tabs
import -window root "$T/tabs.png"
P close; sleep 1.5; xdotool key n; sleep 1.5

# 6. a 32-bit program runs notepad.exe: syswow64's, ours; several files are tabs
P32 launch 'notepad.exe "C:\\t\\be.txt" "C:\\t\\sample.c"'
P tabs; P enc 0; P dump 0 'C:\\t\\dump-be'
focus; xdotool key ctrl+Tab; sleep 0.3; xdotool key ctrl+End; xdotool type y; xdotool key BackSpace; xdotool key ctrl+s; sleep 1
P modified 0
P close; sleep 2

# 7. and a 64-bit one: system32's
P launch 'notepad.exe C:\\t\\find.txt'
P close; sleep 2

# 8. ShellExecute of a .txt (the association)
"$WINE" cmd /c start '' 'C:\\t\\assoc.txt' > /dev/null 2>&1
P wait; sleep 1; P class; P dump 0 'C:\\t\\dump-assoc'
import -window root "$T/final.png"
P close; sleep 1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 420 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

# the n-th answer after a command line in the log
after() { C_="$1" N_="${2:-1}" awk '$0 == ENVIRON["C_"] { k++; if (k == ENVIRON["N_"] + 0) { getline; print; exit } }' "$T/log.out"; }
same() { cmp -s "$1" "$2"; }
u16() { iconv -f "$1" -t UTF-8 "$2" | sed '1s/^\xef\xbb\xbf//'; }

# 1
[ "$(after class 1)" = "class=SgNotepadEditor" ] && pass "cmd's \`notepad.exe file\` starts our Notepad" || fail "cmd started: $(after class 1)"
[ "$(after enc 1)" = "enc=0 eol=0" ] && pass "UTF-8 without a BOM, CRLF, recognised" || fail "UTF-8 file: $(after enc 1)"
same "$C/t/dump-utf8" "$T/orig-utf8" && pass "UTF-8 text shown exactly (accents, a check mark, CJK, an emoji, CRLF)" || fail "UTF-8 text differs"
[ "$(after modified 1)" = "modified=1" ] && [ "$(after modified 2)" = "modified=0" ] && pass "an edit marks it modified; Ctrl+S saves" || fail "modified: $(after modified 1) / $(after modified 2)"
same "$C/t/utf8.txt" "$T/orig-utf8" && pass "saving UTF-8 writes back the same bytes" || fail "UTF-8 save changed the bytes: $(od -c "$C/t/utf8.txt" | head -3)"
# 2
[ "$(after tabs 2)" = "tabs=2 active=1" ] && pass "Ctrl+O opens a second tab" || fail "after Ctrl+O: $(after tabs 2)"
[ "$(after enc 2)" = "enc=2 eol=1" ] && pass "UTF-16 LE with a BOM, LF, recognised" || fail "UTF-16 file: $(after enc 2)"
u16 UTF-16 "$T/orig-utf16" | cmp -s - "$C/t/dump-utf16" && pass "UTF-16 text shown exactly" || fail "UTF-16 text differs"
same "$C/t/utf16.txt" "$T/orig-utf16" && pass "saving UTF-16 LE writes back the same bytes (BOM, LF)" || fail "UTF-16 save changed the bytes: $(od -c "$C/t/utf16.txt" | head -3)"
# 3
[ "$(after getsel 1)" = "a=8 b=11" ] && pass "find (as you type) selects the match" || fail "find: $(after getsel 1)"
[ "$(printf 'a cat sat on a mat\r\nline 12 and 345\r\n')" = "$(cat "$C/t/dump-plain" 2>/dev/null)" ] && pass "replace all (plain text)" || fail "replace all: $(cat -A "$C/t/dump-plain" 2>/dev/null)"
[ "$(printf 'a cat sat on a mat\r\nline <12> and <345>\r\n')" = "$(cat "$C/t/dump-regex" 2>/dev/null)" ] && pass "replace all with a regular expression and a group" || fail "regex replace: $(cat -A "$C/t/dump-regex" 2>/dev/null)"
# 4
[ "$(after lang 1)" = "lang=1" ] && pass "sample.c is highlighted as C" || fail "language: $(after lang 1)"
[ "$(after 'style 55' 1)" = "style=2" ] && [ "$(after 'style 103' 1)" = "style=1" ] && pass "return is a keyword, int a type" || fail "keyword styles: $(after 'style 55' 1) $(after 'style 103' 1)"
[ "$(after 'style 22' 1)" = "style=5" ] && pass "/* note */ is a comment" || fail "comment style: $(after 'style 22' 1)"
[ "$(after 'style 12' 1)" = "style=3" ] && pass "<stdio.h> is a string" || fail "include style: $(after 'style 12' 1)"
[ "$(after 'style 59' 1)" = "style=0" ] && pass "an identifier is plain" || fail "identifier style: $(after 'style 59' 1)"
purple() {
    xy=$(after "screenpos $1" 1); x=${xy#x=}; x=${x%% *}; y=${xy##*y=}
    convert "$T/highlight.png" -crop "40x14+$x+$((y + 3))" +repage \
        -fx '(b > r + 0.12 && b > g + 0.25 && r > g + 0.08) ? 1 : 0' -format '%[fx:int(mean*w*h+0.5)]' info: 2>/dev/null | tail -n 1
}
kw=$(purple 103); id=$(purple 59)
[ "${kw:-0}" -ge 8 ] && [ "${id:-99}" -le 1 ] && pass "on the screen: the keyword is purple ($kw px), an identifier is not ($id px)" || fail "pixels: keyword $kw, identifier $id"
# 5
[ "$(after tabs 4)" = "tabs=4 active=3" ] && [ "$(after tabs 5)" = "tabs=5 active=4" ] && pass "Ctrl+N opens a new tab" || fail "Ctrl+N: $(after tabs 4) -> $(after tabs 5)"
[ "$(after tabs 6)" = "tabs=5 active=0" ] && pass "Ctrl+Tab goes round to the first tab" || fail "Ctrl+Tab: $(after tabs 6)"
[ "$(after tabs 7)" = "tabs=4 active=0" ] && [ "$(after tabs 8)" = "tabs=3 active=0" ] && pass "Ctrl+W closes tabs" || fail "Ctrl+W: $(after tabs 7) / $(after tabs 8)"
# 6
case "$(after '32 launch notepad.exe "C:\t\be.txt" "C:\t\sample.c"' 1)" in
    *'\syswow64\notepad.exe editor=SgNotepadEditor bits=32') pass "a 32-bit program's notepad.exe is syswow64's, and ours" ;;
    *) fail "32-bit launch: $(after '32 launch notepad.exe "C:\t\be.txt" "C:\t\sample.c"' 1)" ;; esac
[ "$(after tabs 9)" = "tabs=2 active=1" ] && pass "two quoted files on the command line are two tabs" || fail "command-line tabs: $(after tabs 9)"
[ "$(after 'enc 0' 1)" = "enc=3 eol=2" ] && pass "UTF-16 BE with a BOM, CR (old Mac), recognised" || fail "UTF-16 BE file: $(after 'enc 0' 1)"
u16 UTF-16 "$T/orig-be" | cmp -s - "$C/t/dump-be" && pass "UTF-16 BE text shown exactly" || fail "UTF-16 BE text differs"
[ "$(after 'modified 0' 1)" = "modified=0" ] && same "$C/t/be.txt" "$T/orig-be" && pass "saving UTF-16 BE writes back the same bytes (BOM, CR, no final line end)" || fail "UTF-16 BE save: $(od -c "$C/t/be.txt" | head -3)"
# 7
case "$(after 'launch notepad.exe C:\t\find.txt' 1)" in
    *'\system32\notepad.exe editor=SgNotepadEditor bits=64') pass "a 64-bit program's notepad.exe is system32's, and ours" ;;
    *) fail "64-bit launch: $(after 'launch notepad.exe C:\t\find.txt' 1)" ;; esac
# 8
[ "$(after class 2)" = "class=SgNotepadEditor" ] && same "$C/t/dump-assoc" "$C/t/assoc.txt" && pass "opening a .txt (ShellExecute) opens it in our Notepad" || fail "association: $(after class 2)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
