#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The display scale changes while programs run (0890): Settings > Display >
# Scale writes LogPixels and broadcasts WM_SETTINGCHANGE "WindowMetrics";
# Wine then does what Windows does (David 2026-10-05: "it'd be nice if we
# could do so while logged in without having to restart the session"). In
# the shell under Xvfb at 2736x1824 (a Surface Pro 7's), started at 100%,
# with dpilive-probe.c's windows, each a 400x300 client area of its own
# colour, measured on the screen:
#   1. at 100%: every window 400x300
#   2. Settings' change to 175% (LogPixels 168):
#      - per-monitor v2: WM_DPICHANGED with 168 and a suggested rectangle
#        whose client area is 700x525; its child hears
#        WM_DPICHANGED_BEFOREPARENT before and _AFTERPARENT after; it is
#        700x525 on the screen; its window and monitor DPI 168
#      - per-monitor v1: WM_DPICHANGED with 168, no child messages; about
#        700x525 (the suggested rectangle scales the whole window)
#      - DPI unaware, and system aware started at 100%: no WM_DPICHANGED,
#        still 96 DPI to themselves, and Wine draws them 700x525 (1.75
#        times, not 2)
#      - a system-aware program started now: system DPI 168, 700x525
#      - a layered window (per-pixel alpha) is scaled with a filter: its
#        red and blue halves meet in blended pixels, not stepped ones
#      - the taskbar is the new scale's: 70 px
#      - File Explorer (laid out at the system DPI) open at the change:
#        1.75 times its size
#   3. back to 100%: per-monitor v2 told 96 and 400x300; the unaware window
#      400x300; the one started at 175% scaled down to 400x300; the
#      taskbar 40 px
#
#   WINE=/opt/wine-sg/bin/wine test/dpilive-gate.sh
#   (mutants SG_MUTANT_DPI_LIVE in dlls/win32u/sysparams.c: nothing follows;
#   SG_MUTANT_DPI_V2_FRAME in dlls/win32u/defwnd.c: a v2 frame not scaled;
#   SG_MUTANT_DPI_V1_ONLY in server/window.c: v2 made v1, no child messages;
#   SG_MUTANT_FE_PM in programs/explorer/fileexplorer.c: File Explorer not
#   scaled; SG_MUTANT_DPI_STRETCH_HALFTONE in dlls/win32u/dce.c: StretchBlt
#   as before, a layered window stretched unfiltered)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb import python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3 PIL missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dpilive.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/dpilive-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/bar.exe" "$HERE/barscale-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
W=2736 H=1824
Xvfb -displayfd 3 -screen 0 "${W}x${H}x24" -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 96 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer "/desktop=shell,${W}x$H" >/dev/null 2>&1 &
sleep 8

# the client areas, by colour: NAME=WxH for each
measure() {
    import -window root "$T/shot.png" 2>/dev/null
    python3 - "$T/shot.png" <<'EOS'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); w, h = im.size; px = im.load()
cols = {(0x10, 0xa0, 0x50): 'v2', (0xa0, 0x10, 0x50): 'unaware', (0x10, 0x50, 0xa0): 'system',
        (0xa0, 0xa0, 0x10): 'v1', (0x50, 0xa0, 0xa0): 'later'}
box = {}
for y in range(0, h, 1):
    for x in range(0, w, 1):
        n = cols.get(px[x, y])
        if n:
            b = box.setdefault(n, [x, y, x, y])
            if x < b[0]: b[0] = x
            if x > b[2]: b[2] = x
            if y > b[3]: b[3] = y
print(' '.join('%s=%dx%d' % (n, b[2] - b[0] + 1, b[3] - b[1] + 1) for n, b in sorted(box.items())))
EOS
}
size_of() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
near() {   # WxH W H TOLERANCE
    awk -v s="$1" -v w="$2" -v h="$3" -v t="$4" 'BEGIN { split(s, a, "x"); d1 = a[1] - w; d2 = a[2] - h;
        exit !(a[1] > 0 && d1 <= t && d1 >= -t && d2 <= t && d2 >= -t) }'
}
bar() { "$WINE" "$T/bar.exe" bar 2>/dev/null | tr -d '\r' | sed -n 's/.*height=\([0-9]*\).*/\1/p'; }
last_state() { grep '^state' "$T/$1.log" | tail -1; }

# File Explorer, aware of the system DPI, below the probes' windows
"$WINE" explorer 'C:\\' >/dev/null 2>&1 &
sleep 6
fe1=$("$WINE" "$T/probe.exe" rect ExplorerWClass bottom 2>/dev/null | tr -d '\r')
"$WINE" "$T/probe.exe" win pmv2 100 100 10a050 'Z:'"$T/v2.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win unaware 700 100 a01050 'Z:'"$T/unaware.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win system 100 450 1050a0 'Z:'"$T/system.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win pmv1 1400 100 a0a010 'Z:'"$T/v1.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" layered 1220 800 >/dev/null 2>&1 &
sleep 6
m=$(measure); b1=$(bar)
ok=1; for n in v2 unaware system v1; do near "$(size_of "$m" $n)" 400 300 0 || ok=0; done
[ $ok = 1 ] && [ "${b1:-0}" = 40 ] && pass "at 100%: every window 400x300 ($m), the taskbar 40 px" \
    || fail "at 100%: $m, taskbar ${b1:-none} (want 400x300 each, 40 px)"

# --- Settings' change to 175% ------------------------------------------------------------
"$WINE" "$T/probe.exe" set 168 >/dev/null 2>&1
sleep 4
m=$(measure)
v2=$(grep '^dpichanged' "$T/v2.log" | tail -1)
order=$(grep -E '^(dpichanged|child)' "$T/v2.log" | awk '{print $1 ($1 == "child" ? "-" $2 : "")}' | tr '\n' ' ')
set -- $v2
if [ "${2:-}" = 168 ] && [ "$order" = "child-before dpichanged child-after " ] && near "$(size_of "$m" v2)" 700 525 1 &&
   last_state v2 | grep -q 'win=168 sys=96 mon=168 caps=96 client=700x525 v2=1'; then
    pass "per-monitor v2 at 175%: WM_DPICHANGED 168, suggested ${5:-?}x${6:-?} at ${3:-?},${4:-?}; its child before and after ($order); $(size_of "$m" v2) on the screen; $(last_state v2)"
else
    fail "per-monitor v2 at 175%: '$v2', messages '$order', $(size_of "$m" v2) on the screen (want 700x525), $(last_state v2)"
fi
v1=$(grep '^dpichanged' "$T/v1.log" | tail -1)
if echo "$v1" | grep -q '^dpichanged 168 ' && ! grep -q '^child' "$T/v1.log" && near "$(size_of "$m" v1)" 700 525 1; then
    pass "per-monitor v1 at 175%: '$v1', no child messages, $(size_of "$m" v1) on the screen"
else
    fail "per-monitor v1 at 175%: '$v1', $(grep -c '^child' "$T/v1.log") child messages, $(size_of "$m" v1) (want about 700x525)"
fi
for n in unaware system; do
    if ! grep -q '^dpichanged' "$T/$n.log" && near "$(size_of "$m" $n)" 700 525 2 && last_state $n | grep -q 'win=96 sys=96 mon=96 caps=96 client=400x300'; then
        pass "$n at 175%: not told, still 96 DPI to itself ($(last_state $n)), drawn $(size_of "$m" $n) by Wine"
    else
        fail "$n at 175%: $(grep -c '^dpichanged' "$T/$n.log") WM_DPICHANGED, $(last_state $n), drawn $(size_of "$m" $n) (want 700x525)"
    fi
done
# the layered window (per-pixel alpha, DPI unaware) at 175%: filtered where
# its red half meets its blue one, not stretched pixel by pixel
cp "$T/shot.png" "$T/shot175.png"
blend=$(python3 - "$T/shot175.png" <<'EOS'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); px = im.load()
red, blue = (0xc0, 0x20, 0x20), (0x20, 0x20, 0xc0)
y = 1487
row = [px[x, y] for x in range(2100, 2520)]
xs = [i for i, c in enumerate(row) if c in (red, blue)]
if not xs: print('none'); sys.exit()
seg = row[xs[0]:xs[-1] + 1]
mixed = [c for c in seg if c not in (red, blue) and c[1] == 0x20 and c[0] > 0x20 and c[2] > 0x20]
print('%d %d %d' % (sum(c == red for c in seg), sum(c == blue for c in seg), len(mixed)))
EOS
)
set -- $blend
if [ "${3:-0}" -ge 1 ] && [ "${1:-0}" -ge 170 ] && [ "${2:-0}" -ge 170 ]; then
    pass "a layered window (per-pixel alpha) at 175%: $1 red and $2 blue pixels across, $3 blended between (filtered)"
else
    fail "a layered window at 175%: '$blend' (red, blue, blended pixels across; want about 175 each and blended ones between)"
fi
"$WINE" "$T/probe.exe" win system 1400 700 50a0a0 'Z:'"$T/later.log" >/dev/null 2>&1 &
sleep 4
m=$(measure); b2=$(bar)
near "$(size_of "$m" later)" 700 525 0 && last_state later | grep -q 'win=168 sys=168' \
    && pass "a system-aware program started at 175%: $(last_state later), $(size_of "$m" later) on the screen" \
    || fail "a system-aware program started at 175%: $(last_state later), $(size_of "$m" later) (want 700x525 at 168)"
[ "${b2:-0}" = 70 ] && pass "the taskbar at 175%: 70 px" || fail "the taskbar at 175%: ${b2:-none} px (want 70)"
fe2=$("$WINE" "$T/probe.exe" rect ExplorerWClass 2>/dev/null | tr -d '\r')
near "$fe2" "$(( ${fe1%x*} * 7 / 4 ))" "$(( ${fe1#*x} * 7 / 4 ))" 3 2>/dev/null \
    && pass "File Explorer, open at the change: $fe1 -> $fe2 on the screen (Wine scales it, as Windows does a program aware of the system DPI)" \
    || fail "File Explorer, open at the change: ${fe1:-none} -> ${fe2:-none} (want 1.75 times)"

# --- and back to 100% --------------------------------------------------------------------
"$WINE" "$T/probe.exe" set 96 >/dev/null 2>&1
sleep 4
m=$(measure); b3=$(bar)
v2=$(grep '^dpichanged' "$T/v2.log" | tail -1)
echo "$v2" | grep -q '^dpichanged 96 ' && near "$(size_of "$m" v2)" 400 300 1 \
    && pass "per-monitor v2 back at 100%: '$v2', $(size_of "$m" v2)" \
    || fail "per-monitor v2 back at 100%: '$v2', $(size_of "$m" v2) (want 400x300)"
near "$(size_of "$m" unaware)" 400 300 1 && near "$(size_of "$m" later)" 400 300 3 \
    && pass "back at 100%: the unaware window $(size_of "$m" unaware), the one started at 175% scaled down to $(size_of "$m" later)" \
    || fail "back at 100%: unaware $(size_of "$m" unaware), started at 175% $(size_of "$m" later) (want 400x300)"
[ "${b3:-0}" = 40 ] && pass "the taskbar back at 100%: 40 px" || fail "the taskbar back at 100%: ${b3:-none} px (want 40)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
