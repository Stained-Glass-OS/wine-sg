#!/bin/sh
# Notepad's minimap (patches/sg/0100): a strip right of the text showing the
# whole document in miniature, the lines on screen shaded; a click centres
# the editor on the line under it, dragging keeps following. Under Xvfb, with
# the X mouse.
#
#  - a C file shows it (the default: code files), its pixels are the lexer's
#    colours (comment green, keyword purple) on its own background, and the
#    shaded band is where the editor is;
#  - a click on the strip moves the editor's first visible line so the line
#    under the pointer is in the middle; the shaded band follows;
#  - a drag keeps moving it;
#  - a 300 000-line file: the strip scrolls with the editor, a click at its
#    foot reaches the end, in bounded time;
#  - the dark theme draws it dark;
#  - a .txt file has none; View > Minimap turns it on (and saves sgMinimap).
#
#   WINE=/opt/wine-sg/bin/wine test/notepad-minimap-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   ARTIFACTS=DIR keeps screenshots
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${NOTEPAD_MINIMAP_DPY:-199}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdotool import python3 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-notepad-minimap.XXXXXX); chmod 755 "$T"
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

Xvfb ":$DPY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cat > "$T/probe.c" <<'EOF'
#include <windows.h>
#include <stdio.h>
#define SGE_GETTOPLINE   (WM_USER + 0x504)
#define SGE_GETMINIMAP   (WM_USER + 0x505)
#define SGE_GETMMLINE    (WM_USER + 0x506)
#define SGE_GETVISROWS   (WM_USER + 0x507)
/* state: the editor's first line, rows, and the minimap in screen terms */
int wmain(int argc, WCHAR **argv)
{
    HWND w = FindWindowW(L"Notepad", NULL), ed;
    LRESULT mm;
    RECT rc;
    POINT o = { 0, 0 };
    if (!w) { puts("NOWINDOW"); return 1; }
    if (argc > 1 && !wcscmp(argv[1], L"close")) { PostMessageW(w, WM_CLOSE, 0, 0); return 0; }
    if (argc > 1 && !wcscmp(argv[1], L"front")) { SetForegroundWindow(w); return 0; }
    ed = FindWindowExW(w, NULL, L"SgNotepadEditor", NULL);
    while (ed && !IsWindowVisible(ed)) ed = FindWindowExW(w, ed, L"SgNotepadEditor", NULL);
    if (!ed) { puts("NOEDITOR"); return 1; }
    mm = SendMessageW(ed, SGE_GETMINIMAP, 0, 0);
    if (argc > 2 && !wcscmp(argv[1], L"line"))
    {
        printf("line %ld\n", (long)SendMessageW(ed, SGE_GETMMLINE, _wtoi(argv[2]), 0));
        return 0;
    }
    GetClientRect(ed, &rc);
    ClientToScreen(ed, &o);
    printf("top %ld\nvis %ld\n", (long)SendMessageW(ed, SGE_GETTOPLINE, 0, 0), (long)SendMessageW(ed, SGE_GETVISROWS, 0, 0));
    if (mm) printf("minimap %ld %ld %ld %ld %d\n", o.x + LOWORD(mm), o.y, o.x + rc.right, o.y + rc.bottom, HIWORD(mm));
    else puts("minimap none");
    return 0;
}
EOF
"$MINGW" -municode -O2 -o "$C/probe.exe" "$T/probe.c" -luser32 || { fail "probe did not build"; exit 1; }
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg 'HKCU\Software\Wine\Explorer' /v Desktop /d shell
reg 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700
reg 'HKCU\Software\Microsoft\Notepad' /v sgTheme /t REG_DWORD /d 1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
mkdir -p "$C/t"
python3 - "$C/t" <<'EOF'
import sys
d = sys.argv[1]
lines = []
for i in range(2000):
    lines.append("/* block %d: a comment line that is long enough to show */" % i)
    lines.append("int function_%d(int value)" % i)
    lines.append("{")
    lines.append("    return value + %d;" % i)
    lines.append("}")
open(d + "/code.c", "w", newline="").write("\r\n".join(lines) + "\r\n")
open(d + "/huge.c", "w", newline="").write("".join("int v%d = %d; /* line %d */\n" % (i, i, i) for i in range(300000)))
open(d + "/small.c", "w", newline="").write("".join("int small_%d = %d; /* a short file */\r\n" % (i, i) for i in range(150)))
open(d + "/plain.txt", "w", newline="").write("".join("plain text line %d\r\n" % i for i in range(500)))
EOF

np_running() { pgrep -x notepad.exe >/dev/null; }
np_quit() {
    "$WINE" 'C:\probe.exe' close >/dev/null 2>&1
    i=0; while np_running && [ $i -lt 30 ]; do sleep 0.3; i=$((i + 1)); done
    pkill -x notepad.exe
}
st() { "$WINE" 'C:\probe.exe' | tr -d '\r' > "$T/st.out"; }
sv() { sed -n "s/^$1 //p" "$T/st.out"; }
mmline() { "$WINE" 'C:\probe.exe' line "$1" | tr -d '\r' | sed -n 's/^line //p'; }
open_np() {
    "$WINE" notepad.exe "$1" >/dev/null 2>&1 &
    i=0; while ! "$WINE" 'C:\probe.exe' >/dev/null 2>&1 && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 2
    "$WINE" 'C:\probe.exe' front >/dev/null 2>&1; sleep 0.5
}
# pixel statistics of a rectangle: file l t r b -> "colours dark% green purple"
stats() {
    python3 - "$@" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
l, t, r, b = map(int, sys.argv[2:6])
px = [im.getpixel((x, y)) for y in range(t, b) for x in range(l, r)]
green = sum(1 for p in px if p[1] > p[0] + 30 and p[1] > p[2] + 20)
purple = sum(1 for p in px if p[2] > p[1] + 40 and p[0] > p[1] + 20)
dark = sum(1 for p in px if sum(p) < 200)
print(len(set(px)), int(100 * dark / max(1, len(px))), green, purple)
EOF
}
# the shaded band's rows in the strip: file l t r b -> first last (by the strip's commonest two backgrounds)
band() {
    python3 - "$@" <<'EOF'
import sys
from collections import Counter
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
l, t, r, b = map(int, sys.argv[2:6])
x = r - 3                                   # the right margin: background only
col = [im.getpixel((x, y)) for y in range(t, b)]
c = Counter(col).most_common(2)
if len(c) < 2: print(-1, -1); sys.exit()
bg, other = c[0][0], c[1][0]
# the band is the darker of the two in a light theme
shade = min((bg, other), key=sum)
rows = [y for y, p in enumerate(col) if p == shade]
print(rows[0], rows[-1]) if rows else print(-1, -1)
EOF
}

# --- 1. a C file: the minimap is there and drawn ------------------------------------------------------
open_np 'C:\t\code.c'
st
set -- $(sv minimap)
if [ $# -eq 5 ]; then
    L=$1; TOP=$2; R=$3; B=$4
    pass "a C file has a minimap ($((R - L)) px wide)"
    import -window root "$T/code.png"
    set -- $(stats "$T/code.png" "$L" "$TOP" "$R" "$B")
    [ "$1" -ge 6 ] && [ "$3" -ge 200 ] && [ "$4" -ge 50 ] && pass "it draws the code in the lexer's colours ($1 colours, $3 comment-green, $4 keyword-purple px)" \
        || fail "minimap pixels: $1 colours, $3 green, $4 purple"
    LAST=$(mmline $((B - TOP - 1)))
    [ "$LAST" -ge 9900 ] 2>/dev/null && pass "the strip shows the whole 10 000-line document (its last row is line $LAST)" || fail "the strip's last row is line $LAST"
    set -- $(band "$T/code.png" "$L" "$TOP" "$R" "$B")
    [ "$1" -eq 0 ] && pass "the shaded band is at the top, where the editor is (rows $1-$2)" || fail "band at rows $1-$2"
else
    fail "no minimap for a C file: $(cat "$T/st.out")"; L=0; TOP=0; R=0; B=0
fi

# --- 2. a click centres the editor on the line under it; a drag follows ----------------------------------
if [ "$R" -gt 0 ]; then
    X=$(( (L + R) / 2 )); VIS=$(sv vis)
    LN=$(mmline 300)
    xdotool mousemove "$X" $((TOP + 300)) click 1; sleep 1
    st; GOT=$(sv top); WANT=$(( LN - VIS / 2 ))
    [ "$GOT" -ge $((WANT - 2)) ] && [ "$GOT" -le $((WANT + 2)) ] && pass "a click on the strip's row 300 (line $LN) puts the editor's first line at $GOT (want ~$WANT)" \
        || fail "click: first line $GOT, want ~$WANT"
    import -window root "$T/clicked.png"
    set -- $(band "$T/clicked.png" "$L" "$TOP" "$R" "$B")
    [ "$1" -ge 295 ] && [ "$2" -le 306 ] && pass "the shaded band follows (rows $1-$2)" || fail "band after the click: rows $1-$2"
    LN=$(mmline 120)
    xdotool mousemove "$X" $((TOP + 40)) sleep 0.2 mousedown 1 sleep 0.3 mousemove "$X" $((TOP + 80)) sleep 0.3 mousemove "$X" $((TOP + 120)) sleep 0.5
    st; D1=$(sv top)
    xdotool mouseup 1; sleep 0.5
    WANT=$(( LN - VIS / 2 ))
    [ "$D1" -ge $((WANT - 2)) ] && [ "$D1" -le $((WANT + 2)) ] && pass "dragging on the strip keeps following (first line $D1, want ~$WANT)" || fail "drag: $D1, want ~$WANT"
fi
np_quit

# --- 2b. a short file: a line per two pixels, a click there ----------------------------------------------
open_np 'C:\t\small.c'
st
set -- $(sv minimap)
if [ $# -eq 5 ] && [ "$5" -ge 2 ]; then
    L=$1; TOP=$2; R=$3; RH=$5
    xdotool mousemove $(( (L + R) / 2 )) $((TOP + 120 * RH + 1)) click 1; sleep 1
    st; WANT=$(( 120 - $(sv vis) / 2 ))
    [ "$(sv top)" -ge $((WANT - 2)) ] && [ "$(sv top)" -le $((WANT + 2)) ] && pass "a short file: $RH px a line, a click on line 120 centres it (first line $(sv top))" \
        || fail "short file click: first line $(sv top), want ~$WANT"
else fail "short file minimap: $(cat "$T/st.out")"; fi
np_quit

# --- 3. a 300 000-line file ------------------------------------------------------------------------
open_np 'C:\t\huge.c'
st
set -- $(sv minimap)
if [ $# -eq 5 ]; then
    L=$1; TOP=$2; R=$3; B=$4
    S0=$(date +%s%N)
    xdotool mousemove $(( (L + R) / 2 )) $((B - 2)) click 1
    i=0; while st && [ "$(sv top)" -lt 290000 ] && [ $i -lt 80 ]; do sleep 0.1; i=$((i + 1)); done
    S1=$(date +%s%N)
    MS=$(( (S1 - S0) / 1000000 ))
    [ "$(sv top)" -ge 290000 ] && pass "a 300 000-line file: a click at the strip's foot goes to the end ($(sv top))" || fail "click at the foot: first line $(sv top)"
    [ "$MS" -le 3000 ] && pass "fast on a large file (${MS} ms, probes included)" || fail "slow: ${MS} ms"
    import -window root "$T/huge.png"
    set -- $(stats "$T/huge.png" "$L" "$TOP" "$R" "$B")
    [ "$1" -ge 4 ] && pass "the whole large file is drawn ($1 colours)" || fail "large file strip: $1 colours"
else fail "no minimap for huge.c"; fi
np_quit

# --- 4. the dark theme -------------------------------------------------------------------------------
reg 'HKCU\Software\Microsoft\Notepad' /v sgTheme /t REG_DWORD /d 2
open_np 'C:\t\code.c'
st
set -- $(sv minimap)
if [ $# -eq 5 ]; then
    import -window root "$T/dark.png"
    set -- $(stats "$T/dark.png" "$1" "$2" "$3" "$4")
    [ "$2" -ge 60 ] && pass "dark theme: the minimap is dark ($2% dark pixels)" || fail "dark theme: minimap $2% dark"
else fail "no minimap in the dark theme"; fi
np_quit
reg 'HKCU\Software\Microsoft\Notepad' /v sgTheme /t REG_DWORD /d 1

# --- 5. plain text: none by default; View > Minimap ---------------------------------------------------
open_np 'C:\t\plain.txt'
st
[ "$(sv minimap)" = none ] && pass "a .txt file has no minimap by default" || fail "plain text: minimap $(sv minimap)"
xdotool key alt+v; sleep 0.8; xdotool key m; sleep 1
st
case "$(sv minimap)" in none|'') fail "View > Minimap did not show it";; *) pass "View > Minimap shows it";; esac
import -window root "$T/plain.png"
np_quit
M=$("$WINE" reg query 'HKCU\Software\Microsoft\Notepad' /v sgMinimap 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_DWORD *//p')
[ "$M" = 0x1 ] && pass "the choice is kept (sgMinimap = 1)" || fail "sgMinimap: '$M'"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
