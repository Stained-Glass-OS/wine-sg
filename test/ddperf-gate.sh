#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A windowed DirectDraw primary is presented at most once a refresh, and its last picture shows
# (7880). 7860 shows a windowed (DDSCL_NORMAL) primary through the swap chain -- the cure for
# the VMR-7's frozen picture on NVIDIA/Xwayland -- but presented the whole window after every
# Blt, BltFast, Unlock or ReleaseDC: a sprite engine's 200 small Blts a frame cost 200 presents
# a frame. Windows' compositor shows a window's changes once a refresh; so does wine-sg now:
# the first update after a quiet refresh is presented at once (a video's frames, a program that
# draws seldom: no added latency), the ones that follow within the refresh are gathered into one
# present at its end, from a timer -- also when the program draws nothing more.
# test/ddperf-probe.c draws, on a 640x480 window at DDSCL_NORMAL with a clipper: frames of 200
# 32x32 Blts to the primary ("small"), the same to an offscreen surface ("ref": the blits' own
# cost), GetDC + 20 TextOut + ReleaseDC frames ("gdi"), whole-window Blts ("full"), then a burst
# of Blts followed at once by a magenta square, and nothing more. Checked:
#   - the small-blit frames present at most twice a refresh (60 Hz: 120 a second; wined3d's
#     frametime trace, one line per present, counted inside the phase). Deterministic: 7860
#     made one present per Blt, thousands a second;
#   - small Blts to the primary run at least 1/SG_DDPERF_FACTOR (default 1/10) the rate of the
#     same Blts to an offscreen surface. Measured (2026-11-19, RTX 3090 weston / llvmpipe Xvfb,
#     Blts a second to the primary, offscreen): 967's front buffer path 5000 / 4700 (1/100);
#     7860 (968) 5300 / 1300 (1/85, 1/340); 7880 105000 / 86000 (1/4.8, 1/4.2) -- 20 and 18
#     times 967's. The rest of the gap is the clipper's clipping, which every Blt pays;
#   - the magenta square drawn last is on the screen within a second and stays.
# Screen: a private headless weston + Xwayland on the host's GPU when there is one, else Xvfb
# (SG_DDPERF_DISPLAY=xvfb forces Xvfb).
#   WINE=/opt/wine-sg/bin/wine test/ddperf-gate.sh
# Mutant (ddraw, -DSG_MUTANT_DDRAW_PRESENT_EACH_UPDATE): every update presented at once again.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
SECS="${DDPERF_SECS:-3}"
FACTOR="${SG_DDPERF_FACTOR:-10}"
for t in "$MINGW" import xwininfo; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ddperf.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -mwindows -o "$T/ddperf-probe.exe" "$HERE/ddperf-probe.c" -lddraw -ldxguid -lgdi32 -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }

mode=xvfb
if [ "${SG_DDPERF_DISPLAY:-}" != xvfb ] && command -v weston >/dev/null && command -v Xwayland >/dev/null; then
    weston --backend=headless --renderer=gl --width=1024 --height=768 --xwayland --idle-time=0 \
        --socket=sg-ddperf-wl > "$T/weston.log" 2>&1 & XP=$!
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
cp "$T/ddperf-probe.exe" "$C/"
(cd "$C" && WINEDEBUG=-all,+timestamp,trace+frametime timeout -s KILL $((SECS * 4 + 60)) "$WINE" explorer /desktop=ddperf,800x600 'C:\ddperf-probe.exe' "$SECS" \
    >"$T/run.log" 2>&1 &)
i=0; while [ "$(cat "$C/phase.txt" 2>/dev/null)" != final ] && [ $i -lt $((SECS * 25 + 600)) ]; do
    case "$(cat "$C/phase.txt" 2>/dev/null)" in failed|done) break ;; esac; sleep 0.2; i=$((i + 1)); done
if [ "$(cat "$C/phase.txt" 2>/dev/null)" != final ]; then
    echo "FAIL  the probe did not get to its last picture: $(cat "$C/probe.log" 2>/dev/null)"; exit 1
fi
WID=$(xwininfo -root -tree | awk '/"ddperf - Wine Desktop"/ {print $1; exit}')
[ -n "$WID" ] || { echo "FAIL  no desktop window"; exit 1; }
# the square at client (200,150)-(320,270); the window is at the desktop's (0,0)
: > "$T/samples"
i=0; while [ $i -lt 10 ]; do
    import -window "$WID" -crop 1x1+260+210 +repage -depth 8 rgb:- 2>/dev/null | od -An -tx1 | tr -d ' \n' >> "$T/samples"
    echo >> "$T/samples"; sleep 0.25; i=$((i + 1))
done
i=0; while [ "$(cat "$C/phase.txt" 2>/dev/null)" = final ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i + 1)); done
"$WINESERVER" -k 2>/dev/null

fail=0
log=$(tr -d '\r' < "$C/probe.log")
val() { printf '%s\n' "$log" | sed -n "s/^$1 blits \([0-9]*\) frames \([0-9]*\) ms \([0-9]*\)$/\\$2/p"; }
for p in small ref gdi full; do
    b=$(val $p 1); f=$(val $p 2); ms=$(val $p 3)
    [ -n "$ms" ] && [ "$ms" -gt 0 ] || { echo "FAIL  no $p phase in the probe's log"; fail=1; continue; }
    printf 'INFO  %-5s %8d updates/s %6d frames/s\n' $p $((b * 1000 / ms)) $((f * 1000 / ms))
done
sb=$(val small 1); sms=$(val small 3); rb=$(val ref 1); rms=$(val ref 3)
if [ -n "$sms" ] && [ -n "$rms" ] && [ "$sms" -gt 0 ] && [ "$rms" -gt 0 ]; then
    srate=$((sb * 1000 / sms)); rrate=$((rb * 1000 / rms))
    if [ $((srate * FACTOR)) -ge "$rrate" ]; then
        echo "PASS  small Blts to the primary run at $srate a second, the offscreen ones at $rrate (within 1/$FACTOR)"
    else
        echo "FAIL  small Blts to the primary run at $srate a second, the offscreen ones at $rrate (slower than 1/$FACTOR)"; fail=1
    fi
fi
# presents inside the small phase: frametime lines whose +timestamp (GetTickCount) lies between
# the phase's start and end, as the probe logged them
presents=$(awk -v s="$(printf '%s\n' "$log" | sed -n 's/^small start \([0-9]*\)$/\1/p')" \
    -v e="$(printf '%s\n' "$log" | sed -n 's/^small end \([0-9]*\)$/\1/p')" '
    /trace:frametime:/ { split($1, a, "."); t = a[1] * 1000 + substr(a[2], 1, 3); if (t >= s && t <= e) n++ }
    END { print n + 0 }' "$T/run.log")
if [ -n "$sms" ] && [ "$sms" -gt 0 ]; then
    pps=$((presents * 1000 / sms))
    if [ "$pps" -gt 120 ]; then echo "FAIL  the small-Blt frames were presented $presents times ($pps a second, more than twice a refresh)"; fail=1
    elif [ "$pps" -lt 10 ]; then echo "FAIL  the small-Blt frames were presented only $presents times ($pps a second): the picture did not follow"; fail=1
    else echo "PASS  the small-Blt frames were presented $presents times ($pps a second): once a refresh"; fi
fi
n=$(grep -c . "$T/samples"); m=$(grep -c '^ff00ff$' "$T/samples")
first=$(awk '$1 == "ff00ff" { print NR; exit }' "$T/samples")
printf 'INFO  last picture: %s of %s samples magenta (%s)\n' "$m" "$n" "$(tr '\n' ' ' < "$T/samples")"
if [ "$n" -ge 8 ] && [ -n "$first" ] && [ "$first" -le 4 ] && [ "$m" -eq $((n - first + 1)) ]; then
    echo "PASS  the picture drawn last shows (from sample $first on) while the program draws nothing more"
else
    echo "FAIL  the picture drawn last does not show"; fail=1
fi
[ $fail = 0 ] && { echo "RESULT: PASS ($mode)"; exit 0; }
echo "RESULT: FAIL ($mode)"; exit 1
