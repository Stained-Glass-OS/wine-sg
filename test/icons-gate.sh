#!/bin/sh
# Our own file and folder icons (theme/icons.py, patches/sg/0478): David --
# folders and files "look old", a text file showed Wine's notepad (a wine
# glass). Checks the built modules: shell32's folder (IDI_SHELL_FOLDER, 4)
# is our manila, its text file icon (IDI_SHELL_TEXT_FILE, 152) a page with
# lines, Notepad's icon carries our accent band, every icon has the sizes
# the desktop asks for (96, 48), and .txt files are registered with it.
#
#   WINE=/opt/wine-sg/bin/wine test/icons-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in wrestool icotool python3; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
# the modules: an installed tree (PREFIX/lib/wine) or a build tree (next to ./wine)
D=$(dirname "$WINE")
if [ -d "$D/../lib/wine/x86_64-windows" ]; then L=$D/../lib/wine/x86_64-windows; INF=$D/../share/wine/wine.inf
else L=; INF=$D/loader/wine.inf; fi
mod() { if [ -n "$L" ]; then echo "$L/$1"; else find "$D" -path "*x86_64-windows/$1" | head -1; fi; }
SHELL32=$(mod shell32.dll); NOTEPAD=$(mod notepad.exe)
[ -f "$SHELL32" ] && [ -f "$NOTEPAD" ] || { echo "SKIP: modules not found"; exit 77; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT INT TERM

icon() {  # icon FILE NAME OUT: the .ico of a group icon resource
    wrestool -x -t 14 -n "$2" "$1" -o "$3" 2>/dev/null
    [ -s "$3" ]
}
cat > "$T/look.py" <<'PY'
import sys, io, struct
from PIL import Image
data = open(sys.argv[1], 'rb').read()
n = struct.unpack_from('<H', data, 4)[0]
sizes = {}
for i in range(n):
    w, h, _, _, _, _, size, off = struct.unpack_from('<BBBBHHII', data, 6 + 16 * i)
    sizes[w or 256] = data[off:off + size]
want = int(sys.argv[2])
print("sizes", " ".join(str(s) for s in sorted(sizes)))
im = Image.open(io.BytesIO(data)); im.size = (want, want); im.load(); im = im.convert("RGBA")
for spec in sys.argv[3:]:
    x, y = (int(v) for v in spec.split(","))
    print("px %s %d,%d,%d,%d" % ((spec,) + im.getpixel((x, y))))
PY

icon "$SHELL32" 4 "$T/folder.ico" || fail "shell32 has no folder icon"
out=$(python3 "$T/look.py" "$T/folder.ico" 48 24,30 2>&1)
printf '%s\n' "$out" | sed 's/^/      folder: /'
r=$(printf '%s\n' "$out" | sed -n 's/^px 24,30 \([0-9]*\),\([0-9]*\),\([0-9]*\),.*/\1 \2 \3/p')
set -- $r
[ "${1:-0}" -gt 220 ] && [ "${2:-0}" -gt 170 ] && [ "${3:-255}" -lt 120 ] && pass "the folder is our manila (48 px: $1,$2,$3)" \
    || fail "the folder is not ours (48 px middle: ${r:-none})"
printf '%s\n' "$out" | grep -q '^sizes.* 48 .*96' && pass "and has the desktop's sizes (48, 96)" || fail "folder sizes: $(printf '%s\n' "$out" | head -1)"

icon "$SHELL32" 152 "$T/text.ico" || fail "shell32 has no text file icon"
out=$(python3 "$T/look.py" "$T/text.ico" 48 35,24 24,4 2>&1)
printf '%s\n' "$out" | sed 's/^/      text: /'
line=$(python3 - "$T/text.ico" <<'PY'
import sys, io
from PIL import Image
im = Image.open(sys.argv[1]); im.size = (48, 48); im.load(); im = im.convert("RGBA")
# a row of grey text on the page's lower half
rows = [y for y in range(18, 44) if sum(1 for x in range(14, 34) if im.getpixel((x, y))[0] < 200 and im.getpixel((x, y))[3] > 200) > 10]
print(len(rows))
PY
)
[ "${line:-0}" -ge 3 ] && pass "a text file is a page with lines ($line rows)" || fail "the text file icon has no lines (${line:-0})"

icon "$NOTEPAD" 1 "$T/np.ico" || icon "$NOTEPAD" IDI_NOTEPAD "$T/np.ico" || wrestool -x -t 14 "$NOTEPAD" -o "$T/np.ico" 2>/dev/null
out=$(python3 "$T/look.py" "$T/np.ico" 48 19,6 2>&1)
printf '%s\n' "$out" | sed 's/^/      notepad: /'
r=$(printf '%s\n' "$out" | sed -n 's/^px 19,6 \([0-9]*\),\([0-9]*\),\([0-9]*\),.*/\1 \2 \3/p')
set -- $r
[ "${3:-0}" -gt 150 ] && [ "${2:-255}" -lt 100 ] && pass "Notepad's icon carries our accent band ($1,$2,$3), not Wine's glass" \
    || fail "Notepad's icon is not ours (band: ${r:-none})"

if [ -f "$INF" ]; then
    grep -q 'HKCR,txtfile\\DefaultIcon,,2,"%11%\\shell32.dll,-152"' "$INF" && pass ".txt files are registered with it" \
        || fail "txtfile has no DefaultIcon in wine.inf"
else
    echo "info  no wine.inf at $INF"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
