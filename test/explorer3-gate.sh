#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer, round 3 (patches/sg/0244-0246), in a shell session under Xvfb.
#
#  - videos have thumbnails: a frame of an H.264 MP4 (red over blue), a VP8
#    WebM (green) and an MPEG-4 AVI (yellow), through IShellItemImageFactory
#    and on the screen in a Large icons view (0244);
#  - a PDF's thumbnail is its first page, from sg-session's sg-pdf (poppler)
#    -- red over blue, portrait (0244);
#  - thumbnails are kept on disk (0245): the entry appears under
#    %LOCALAPPDATA%\Microsoft\Windows\Explorer\sg-thumbcache, a picture made
#    from a doctored entry proves it is read back, a changed file is made
#    again, and the folder keeps under its limit (ThumbnailCacheKB) with the
#    newest entry kept;
#  - the details pane says a picture's dimensions and bit depth and a
#    video's length and frame size (0246);
#  - "Remove from Quick access" on a frequent folder in the navigation pane
#    takes it out for good; a pin dragged in the navigation pane moves among
#    the pins (0246).
#
#   WINE=/opt/wine-sg/bin/wine test/explorer3-gate.sh
#   WINESERVER=... when not beside $WINE (a build tree: obj/server/wineserver)
#   SG_PDF_HELPER=sg-session's bin/sg-pdf (default: ../../sg-session/bin/sg-pdf, /usr/bin/sg-pdf)
#   ARTIFACTS=DIR keeps screenshots and logs
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${EXPLORER3_DPY:-153}"
PDFH="${SG_PDF_HELPER:-}"
[ -n "$PDFH" ] || for c in "$HERE/../../sg-session/bin/sg-pdf" /usr/bin/sg-pdf; do [ -f "$c" ] && { PDFH=$(readlink -f "$c"); break; }; done
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdotool import ffmpeg "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
/usr/bin/python3 -c 'import PIL, cairo' 2>/dev/null || { echo "SKIP: needs python3-pil and python3-cairo"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -n "$PDFH" ] && [ -f "$PDFH" ] || { echo "SKIP: no sg-pdf (SG_PDF_HELPER)"; exit 77; }
PY=/usr/bin/python3

T=$(mktemp -d /var/tmp/sg-explorer3.XXXXXX); chmod 755 "$T"
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/explorer-probe.exe" "$C/"
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg 'HKCU\Software\Wine\Explorer' /v Desktop /d shell
reg 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800
reg 'HKCU\Software\Stained Glass\Explorer' /v Pane /t REG_DWORD /d 1
"$WINESERVER" -w

U=$(ls "$C/users" | grep -v -e '^Public$' | head -1)
UD="$C/users/$U"
for d in Desktop Documents Downloads Music Pictures Videos; do [ -L "$UD/$d" ] && rm -f "$UD/$d"; mkdir -p "$UD/$d"; done
VID="$UD/Videos"; PIC="$UD/Pictures"; DOC="$UD/Documents"
mkdir -p "$DOC/Often" "$DOC/Other" "$PIC/many"
ffmpeg -loglevel error -y -f lavfi -i "color=c=red:s=320x240:d=3,drawbox=x=0:y=120:w=320:h=120:color=blue:t=fill" -c:v libx264 -pix_fmt yuv420p -t 3 "$VID/red-blue.mp4"
ffmpeg -loglevel error -y -f lavfi -i "color=c=green:s=320x180:d=3" -c:v libvpx -t 3 "$VID/green.webm"
ffmpeg -loglevel error -y -f lavfi -i "color=c=yellow:s=320x240:d=3" -c:v mpeg4 -t 3 "$VID/yellow.avi"
$PY - "$DOC" "$PIC" <<'EOF'
import sys, cairo
from PIL import Image
s = cairo.PDFSurface(sys.argv[1] + "/report.pdf", 612, 792)
c = cairo.Context(s)
c.set_source_rgb(1, 0, 0); c.rectangle(0, 0, 612, 396); c.fill()
c.set_source_rgb(0, 0, 1); c.rectangle(0, 396, 612, 396); c.fill()
c.show_page(); s.finish()
Image.new('RGB', (400, 300), (0x20, 0xc0, 0x40)).save(sys.argv[2] + '/sample.png')
for i in range(40):
    Image.new('RGB', (64, 64), (i * 6, 100, 200 - i * 4)).save(sys.argv[2] + '/many/p%02d.png' % i)
EOF
WV="C:\\users\\$U\\Videos"; WP="C:\\users\\$U\\Pictures"; WD="C:\\users\\$U\\Documents"
CACHE="$UD/AppData/Local/Microsoft/Windows/Explorer/sg-thumbcache"

Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" SG_PDF="$PDFH"
P() { "$WINE" 'C:\explorer-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
shot() { sleep 1; import -window root "$T/$1.png"; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/desktop.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/desktop.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
hexcol() { # "rrggbb" -> r g b
    echo "$1" | sed 's/\(..\)\(..\)\(..\)/0x\1 0x\2 0x\3/'
}
near() { # hex expected-r expected-g expected-b: every channel within 48
    set -- $(hexcol "$1") "$2" "$3" "$4"
    [ $(( $1 > $4 ? $1 - $4 : $4 - $1 )) -le 48 ] && [ $(( $2 > $5 ? $2 - $5 : $5 - $2 )) -le 48 ] && [ $(( $3 > $6 ? $3 - $6 : $6 - $3 )) -le 48 ]
}
thumbv() { sed -n 's/^thumb=\([0-9x]*\) top=\([0-9a-f]*\) bottom=\([0-9a-f]*\)$/\1 \2 \3/p'; }

# --- 1. video thumbnails ------------------------------------------------------------------------
set -- $(P thumb "$WV\\red-blue.mp4" 256 | thumbv)
[ $# -eq 3 ] && near "$2" 255 0 0 && near "$3" 0 0 255 && pass "an H.264 MP4's thumbnail is its frame: red over blue ($1)" || fail "MP4 thumbnail: $*"
set -- $(P thumb "$WV\\green.webm" 256 | thumbv)
[ $# -eq 3 ] && near "$2" 0 128 0 && [ "${1%x*}" -gt "${1#*x}" ] && pass "a VP8 WebM's thumbnail: green, wide ($1)" || fail "WebM thumbnail: $*"
set -- $(P thumb "$WV\\yellow.avi" 256 | thumbv)
[ $# -eq 3 ] && near "$2" 255 255 0 && pass "an MPEG-4 AVI's thumbnail: yellow ($1)" || fail "AVI thumbnail: $*"

# on the screen: Videos at Large icons shows the frames
WINEDEBUG=err+all,trace+explorer "$WINE" explorer "$WV" > "$T/videos.out" 2>&1 &
P wait-title Videos 20 > /dev/null
sleep 5; shot videos
n=$($PY - "$T/videos.png" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
w, h = im.size
px = im.load()
red = sum(1 for y in range(0, h, 2) for x in range(0, w, 2) if px[x, y][0] > 200 and px[x, y][1] < 60 and px[x, y][2] < 60)
blue = sum(1 for y in range(0, h, 2) for x in range(0, w, 2) if px[x, y][2] > 200 and px[x, y][0] < 60 and px[x, y][1] < 60)
yel = sum(1 for y in range(0, h, 2) for x in range(0, w, 2) if px[x, y][0] > 200 and px[x, y][1] > 200 and px[x, y][2] < 60)
print(red, blue, yel)
EOF
)
set -- $n
[ "$1" -ge 300 ] && [ "$2" -ge 300 ] && [ "$3" -ge 300 ] && pass "Videos shows the frames as its icons (red $1, blue $2, yellow $3 px)" || fail "Videos view: red $1 blue $2 yellow $3 px"
P close-all >/dev/null 2>&1

# --- 2. PDF thumbnail --------------------------------------------------------------------------------
set -- $(P thumb "$WD\\report.pdf" 256 | thumbv)
[ $# -eq 3 ] && near "$2" 255 0 0 && near "$3" 0 0 255 && [ "${1%x*}" -lt "${1#*x}" ] && pass "a PDF's thumbnail is its first page, from sg-pdf: red over blue, portrait ($1)" || fail "PDF thumbnail: $*"

# --- 3. the cache on disk -----------------------------------------------------------------------------
set -- $(P thumb "$WP\\sample.png" 96 | thumbv)
# the entry whose stored path is sample.png's (Videos' own 96 px entries are there too)
F=$($PY - "$CACHE" <<'EOF'
import glob, sys
for f in glob.glob(sys.argv[1] + '/*_96.thumb'):
    if 'sample.png'.encode('utf-16-le') in open(f, 'rb').read(): print(f)
EOF
)
[ -n "$F" ] && pass "the thumbnail is kept on disk ($(basename "$F"))" || fail "no cache entry under $CACHE: $(ls "$CACHE" 2>&1 | head -3)"
if [ -n "$F" ]; then
    # doctor the entry's pixels (magenta): a picture made from the entry shows it
    $PY - "$F" <<'EOF'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
w, h = struct.unpack_from('<II', d, 8)
plen = struct.unpack_from('<I', d, 32)[0]    # the header: 40 bytes, the path's length at 32
off = 40 + plen * 2
for i in range(w * h):
    d[off + i * 4: off + i * 4 + 4] = bytes((0xff, 0x00, 0xff, 0xff))
open(sys.argv[1], 'wb').write(d)
EOF
    set -- $(P thumb "$WP\\sample.png" 96 | thumbv)
    [ $# -eq 3 ] && [ "$2" = ff00ff ] && pass "the next thumbnail comes from the cache (the doctored entry's magenta)" || fail "cache not read back: $*"
    touch "$PIC/sample.png"
    set -- $(P thumb "$WP\\sample.png" 96 | thumbv)
    [ $# -eq 3 ] && near "$2" 0x20 0xc0 0x40 && pass "a changed file's thumbnail is made again (green, not the stale entry)" || fail "stale cache entry used: $*"
fi
# the limit: 16 KB, 40 new pictures at 32 px (4 KB each)
reg 'HKCU\Software\Stained Glass\Explorer' /v ThumbnailCacheKB /t REG_DWORD /d 16
for i in $(seq -w 0 39); do P thumb "$WP\\many\\p$i.png" 32 >/dev/null; done
total=$(du -cb "$CACHE"/*.thumb 2>/dev/null | tail -1 | cut -f1)
[ "${total:-999999}" -le 16384 ] && [ "${total:-0}" -gt 0 ] && pass "the cache keeps under its limit ($total bytes <= 16 KB)" || fail "cache is $total bytes, limit 16 KB"
last=$(grep -l 'p39.png' "$CACHE"/*.thumb 2>/dev/null | head -1)
[ -z "$last" ] && last=$($PY - "$CACHE" <<'EOF'
import glob, sys
for f in glob.glob(sys.argv[1] + '/*.thumb'):
    if 'p39.png'.encode('utf-16-le') in open(f, 'rb').read(): print(f)
EOF
)
[ -n "$last" ] && pass "the newest entry is kept when older ones go" || fail "the newest entry was pruned"
reg 'HKCU\Software\Stained Glass\Explorer' /v ThumbnailCacheKB /t REG_DWORD /d 102400

# --- 4. the details pane -------------------------------------------------------------------------------
WINEDEBUG=err+all,trace+explorer "$WINE" explorer "/select,$WP\\sample.png" > "$T/pane-pic.out" 2>&1 &
P wait-title Pictures 20 > /dev/null; sleep 4; shot pane-picture
grep -q 'pane line: L"Dimensions:\\t400 x 300"' "$T/pane-pic.out" && pass "details pane: a picture's dimensions (400 x 300)" || fail "no dimensions: $(grep 'pane line' "$T/pane-pic.out" | tail -5 | tr '\n' ' ')"
grep -q 'pane line: L"Bit depth:\\t24"' "$T/pane-pic.out" && pass "details pane: its bit depth (24)" || fail "no bit depth: $(grep 'pane line' "$T/pane-pic.out" | tail -3 | tr '\n' ' ')"
P close-all >/dev/null 2>&1
WINEDEBUG=err+all,trace+explorer "$WINE" explorer "/select,$WV\\red-blue.mp4" > "$T/pane-vid.out" 2>&1 &
P wait-title Videos 20 > /dev/null; sleep 4; shot pane-video
grep -q 'pane line: L"Length:\\t00:00:03"' "$T/pane-vid.out" && grep -q 'pane line: L"Frame width:\\t320"' "$T/pane-vid.out" &&
    grep -q 'pane line: L"Frame height:\\t240"' "$T/pane-vid.out" && pass "details pane: a video's length and frame size (00:00:03, 320 x 240)" ||
    fail "video details: $(grep 'pane line' "$T/pane-vid.out" | tail -6 | tr '\n' ' ')"
P close-all >/dev/null 2>&1
reg 'HKCU\Software\Stained Glass\Explorer' /v Pane /t REG_DWORD /d 0

# --- 5. Quick access --------------------------------------------------------------------------------------
# a frequent folder: visited twice
for k in 1 2; do
    WINEDEBUG=err+all "$WINE" explorer "$WD\\Often" >/dev/null 2>&1 &
    P wait-title Often 20 >/dev/null; sleep 1; P close-all >/dev/null 2>&1; sleep 1
done
WINEDEBUG=err+all,trace+explorer "$WINE" explorer "$WD\\Other" > "$T/quick.out" 2>&1 &
P wait-title Other 20 >/dev/null; sleep 3
row() { grep "quick access row: $1 " "$T/quick.out" | grep -i "$2\"" | tail -1 | awk '{print $(NF-2), $(NF-1)}'; }
set -- $(row 0 'Often')
if [ $# -eq 2 ]; then
    pass "Often is a frequent folder in the navigation pane"
    set -- $(P origin | sed 's/origin=//; s/,/ /') $1 $2
    xdotool mousemove $(($1 + $3)) $(($2 + $4)) click 3; sleep 1.5; shot remove-menu
    xdotool key Down Return; sleep 2
    grep -q 'quick access removed: L".*Often"' "$T/quick.out" && pass "Remove from Quick access (its context menu's first item) takes it out" || fail "not removed: $(grep 'quick access' "$T/quick.out" | tail -3 | tr '\n' ' ')"
    ex=$("$WINE" reg query 'HKCU\Software\Stained Glass\Explorer\QuickAccess' /v Excluded 2>/dev/null | tr -d '\r' | grep -c Often)
    fr=$("$WINE" reg query 'HKCU\Software\Stained Glass\Explorer\QuickAccess\Frequent' 2>/dev/null | tr -d '\r' | grep -c Often)
    [ "$ex" -ge 1 ] && [ "$fr" = 0 ] && pass "it is excluded and its count is gone" || fail "registry: excluded $ex, frequent $fr"
    shot removed
    P close-all >/dev/null 2>&1; sleep 1
    for k in 1 2 3; do
        WINEDEBUG=err+all "$WINE" explorer "$WD\\Often" >/dev/null 2>&1 &
        P wait-title Often 20 >/dev/null; sleep 1; P close-all >/dev/null 2>&1; sleep 1
    done
    WINEDEBUG=err+all,trace+explorer "$WINE" explorer "$WD\\Other" > "$T/quick2.out" 2>&1 &
    P wait-title Other 20 >/dev/null; sleep 3
    grep -q 'quick access row: 0 .*Often"' "$T/quick2.out" && fail "Often came back after more visits" || pass "more visits do not bring it back"
else
    fail "Often is not a frequent folder: $(grep 'quick access row' "$T/quick.out" | tail -6 | tr '\n' ' ')"
    WINEDEBUG=err+all,trace+explorer "$WINE" explorer "$WD\\Other" > "$T/quick2.out" 2>&1 &
    P wait-title Other 20 >/dev/null; sleep 3
fi

# a pin dragged: Pictures (the last default pin) above Desktop (the first)
Q="$T/quick2.out"
row2() { grep "quick access row: 1 " "$Q" | grep -i "$1\"" | tail -1 | awk '{print $(NF-2), $(NF-1)}'; }
set -- $(row2 'Pictures') $(row2 'Desktop')
if [ $# -eq 4 ]; then
    set -- $(P origin | sed 's/origin=//; s/,/ /') "$@"
    xdotool mousemove $(($1 + $3)) $(($2 + $4)) mousedown 1; sleep 0.3
    for s in 1 2 3 4 5 6; do xdotool mousemove $(($1 + $3)) $(($2 + $4 - s * ($4 - $6 + 6) / 6)); sleep 0.15; done
    sleep 0.5; shot dragging-pin; xdotool mouseup 1; sleep 2
    order=$("$WINE" reg query 'HKCU\Software\Stained Glass\Explorer\QuickAccess' /v Pinned 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_MULTI_SZ *//p')
    case "$order" in *Pictures*Desktop*Downloads*Documents*) pass "dragging the Pictures pin above Desktop reorders the pins";; *) fail "pin order: '$order'";; esac
    grep -q 'quick access pin order 0: L".*Pictures"' "$Q" && pass "the navigation pane shows the new order first" || fail "no reorder trace"
    shot pins-reordered
else fail "pin rows not found: $(grep 'quick access row: 1' "$Q" | tail -4 | tr '\n' ' ')"; fi

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
