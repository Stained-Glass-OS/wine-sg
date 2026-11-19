#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A windowed DirectDraw primary surface keeps showing what is blitted to it (7860).
# Wine's default video renderer, the VMR-7, blits every frame to a DirectDraw primary surface
# with a clipper on its video window. wined3d drew those blits into the window's OpenGL front
# buffer, and with NVIDIA's driver under Xwayland such draws stop reaching the window after a
# few frames or at once: the front buffer held every frame, the (composite-redirected) child
# window kept an old one, and the picture froze while the sound played on (Media Player,
# 2026-11-18; sg-media went to the VMR-9 for it). A windowed primary is now shown through the
# swap chain, as the VMR-9's frames are.
# test/ddprimary-probe.c does what the VMR-7 does -- DDSCL_NORMAL, a primary with a clipper on
# its video window (a hidden pop-up for the first frame, then made a child of the program's
# window, as IVideoWindow::put_Owner does), a 960x540 YUY2 flipping surface written with Lock,
# one Blt per frame from a streaming thread at 25 frames a second, the top-level repainting a
# seek bar meanwhile -- and gives each frame its own grey. Checked:
#   - every frame is presented through the swap chain (wined3d's frametime trace: one line per
#     present). Deterministic on every driver: the old build presented none;
#   - the picture on the screen never stands still while frames go on: the middle of the video
#     window, read five times a second, never alike in three samples in a row. When NVIDIA's
#     driver stops delivering the front buffer varies: the old build failed this in about two
#     #     runs of three (20 s), standing still for 3 to 60 samples; the fixed build, never;
#   - the probe keeps its pace (20 frames a second or more: a present must not block it).
# The screen is a private headless weston with Xwayland, its GL renderer on the host's GPU, so
# on an NVIDIA host it is the path a person's desktop takes; Xvfb without weston or with
# SG_DDPRIMARY_DISPLAY=xvfb (Mesa shows the old build's front buffer: only the first check
# tells there).
#   WINE=/opt/wine-sg/bin/wine test/ddprimary-gate.sh
# Mutant (ddraw, -DSG_MUTANT_DDRAW_FRONT_BUFFER_DRAWN): blits drawn into the front buffer again.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
SECS="${DDPRIMARY_SECS:-20}"
for t in "$MINGW" import xwininfo; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ddprimary.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -mwindows -o "$T/ddprimary-probe.exe" "$HERE/ddprimary-probe.c" -lddraw -ldxguid -lgdi32 -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }

mode=xvfb
if [ "${SG_DDPRIMARY_DISPLAY:-}" != xvfb ] && command -v weston >/dev/null && command -v Xwayland >/dev/null; then
    weston --backend=headless --renderer=gl --width=1024 --height=768 --xwayland --idle-time=0 \
        --socket=sg-ddprimary-wl > "$T/weston.log" 2>&1 & XP=$!
    i=0; while ! grep -q 'xserver listening on display' "$T/weston.log" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
    DPY=$(sed -n 's/.*xserver listening on display :\([0-9]*\).*/\1/p' "$T/weston.log" | head -1)
    if [ -n "$DPY" ] && [ "$DPY" != 0 ] && grep -q 'Using GL renderer' "$T/weston.log"; then
        mode=weston
        printf 'INFO  weston + Xwayland on :%s, %s\n' "$DPY" "$(sed -n 's/.*GL renderer: //p' "$T/weston.log" | head -1)"
    else
        kill "$XP" 2>/dev/null; XP=
    fi
fi
if [ $mode = xvfb ]; then
    command -v Xvfb >/dev/null || { echo "SKIP: no weston and no Xvfb"; exit 77; }
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    DPY=$(cat "$T/display")
    printf 'INFO  Xvfb :%s\n' "$DPY"
fi
export DISPLAY=":$DPY"

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/ddprimary-probe.exe" "$C/"
(cd "$C" && WINEDEBUG=-all,trace+frametime timeout -s KILL $((SECS + 60)) "$WINE" explorer /desktop=ddprimary,800x600 'C:\ddprimary-probe.exe' "$SECS" ${DDPRIMARY_PROBE_ARGS:-} \
    >"$T/run.log" 2>&1 &)
i=0; while [ "$(cat "$C/phase.txt" 2>/dev/null)" != running ] && [ $i -lt 300 ]; do
    [ "$(cat "$C/phase.txt" 2>/dev/null)" = failed ] && break; sleep 0.2; i=$((i + 1)); done
if [ "$(cat "$C/phase.txt" 2>/dev/null)" != running ]; then
    echo "FAIL  the probe did not start blitting: $(cat "$C/probe.log" 2>/dev/null)"; exit 1
fi
WID=$(xwininfo -root -tree | awk '/"ddprimary - Wine Desktop"/ {print $1; exit}')
[ -n "$WID" ] || { echo "FAIL  no desktop window"; exit 1; }
sleep 1
: > "$T/samples"
while [ "$(cat "$C/phase.txt" 2>/dev/null)" = running ]; do
    import -window "$WID" -crop 1x1+200+160 +repage -depth 8 rgb:- 2>/dev/null | od -An -tx1 | tr -d ' \n' >> "$T/samples"
    echo >> "$T/samples"
    sleep 0.2
done
"$WINESERVER" -k 2>/dev/null

fail=0
set -- $(tr -d '\r' < "$C/probe.log" | sed -n 's/^frames \([0-9]*\) ms \([0-9]*\)$/\1 \2/p')
if [ $# -eq 2 ] && [ "$2" -gt 0 ]; then
    frames=$1; fps=$(( $1 * 1000 / $2 ))
    if [ "$fps" -ge 20 ]; then echo "PASS  the probe blitted $1 frames in $2 ms ($fps a second)"
    else echo "FAIL  the probe blitted only $1 frames in $2 ms ($fps a second, 25 asked)"; fail=1; fi
else
    echo "FAIL  the probe did not finish: $(cat "$C/probe.log" 2>/dev/null)"; fail=1
fi
tr -d '\r' < "$C/probe.log" 2>/dev/null | grep -v "^frames " | head -3 | sed 's/^/INFO  probe: /'
# each present of a swap chain is a frametime trace line: the frames must be presented, which
# is what reaches the window on every driver (with the front buffer there are none at all)
presents=$(grep -c 'trace:frametime:' "$T/run.log")
if [ "${frames:-0}" -gt 0 ] && [ "$presents" -ge $((frames * 9 / 10)) ]; then
    echo "PASS  the window's frames are presented through the swap chain ($presents presents)"
else
    echo "FAIL  the window's frames are not presented through the swap chain ($presents presents for ${frames:-0} frames)"; fail=1
fi
n=$(grep -c . "$T/samples")
set -- $(awk 'NF { if ($1 == p) run++; else run = 1; if (run > max) max = run; p = $1; if (!($1 in seen)) { seen[$1] = 1; d++ } }
    END { print max + 0, d + 0 }' "$T/samples")
longest=$1; distinct=$2
printf 'INFO  %s samples of the video window, %s different, longest still stretch %s\n' "$n" "$distinct" "$longest"
if [ "$n" -lt 20 ]; then
    echo "FAIL  too few samples of the screen ($n)"; fail=1
elif [ "$longest" -ge 3 ] || [ "$distinct" -lt 10 ]; then
    echo "FAIL  the picture stood still while frames were blitted (as many as $longest samples alike)"; fail=1
else
    echo "PASS  every frame reached the screen: the picture never stood still"
fi
[ $fail = 0 ] && { echo "RESULT: PASS ($mode)"; exit 0; }
echo "RESULT: FAIL ($mode)"; exit 1
