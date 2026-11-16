#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A framed Linux program is given only the sizes its frame has (wine-sg
# 0762 and follow-ups; David 2026-11-15: Kdenlive's and OBS's "UI collapses
# on minimize and maximize" -- Qt lays its docks out again for every size it
# is given, and a tiny one squeezes them to their minimums for good).
# A stand-in (embedconf-app.c) logs every ConfigureNotify it gets while its
# frame is maximized and restored (its frame's button, and asked by the
# program), minimized and restored, snapped (Win+Right, Win+Left back),
# moved to the next virtual desktop and back. Checked for each step:
#   - no size it was given is below its minimum (WM_NORMAL_HINTS) or smaller
#     than both the size before and the size after the step (a transient
#     tiny or wrong size)
#   - after the step its size is its frame's client area
#   - maximized and restored: restored it is the size it had before
#
#   WINE=/opt/wine-sg/bin/wine test/embedconf-gate.sh
# Mutant: SG_MUTANT_EMBED_MINIMIZED_RESIZED (dlls/winex11.drv/sg_embed.c,
# 7770: the program's window sized to 1x1 while minimized) fails minimize.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedconf.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/confapp" "$HERE/embedconf-app.c" -lX11 || { echo "FAIL  embedconf-app did not build"; exit 1; }
cc -O2 -o "$T/netwm-fullscreen" "$HERE/netwm-fullscreen.c" -lX11 || { echo "FAIL  netwm-fullscreen did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${EMBEDCONF_DISPLAY:-}" ]; then
    Xvfb ":$EMBEDCONF_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$EMBEDCONF_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
L="$T/app.log"; : > "$L"
"$T/confapp" "$L" & AP=$!
i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(awk '$1 == "main" {print $2; exit}' "$L")
printf '%d shown - DockTest\tDock Test\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
cd "$WINEPREFIX/drive_c"
root_id=$(xwininfo -root | awk '/Window id:/ {print $4; exit}')
k=0; while [ $k -lt 80 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | awk '/Parent window id:/ {print $4; exit}')
    [ -n "$p" ] && [ "$p" != "$root_id" ] && break
    sleep 0.25; k=$((k + 1))
done
[ $k -lt 80 ] || { fail "the window was not framed"; exit 1; }
sleep 2
W="SgLinuxWindow"; N="Dock Test"
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
# the client area's size as Wine has it = what the program's window should be
size_now() { xwininfo -id "$XID" 2>/dev/null | awk '/^  Width:/ {w=$2} /^  Height:/ {h=$2} END {print w "x" h}'; }
last_conf() { awk '$1 == "conf" {s=$3 "x" $4} END {print s}' "$L"; }
mark() { echo "mark $1" >> "$L"; }
# step NAME: the sizes given since the last mark, against before/after and the minimum
check_step() {
    _name=$1 _before=$2 _after=$3
    _bad=$(awk -v m="mark $_name" -v b="$_before" -v a="$_after" '
        BEGIN { split(b, B, "x"); split(a, A, "x") }
        $0 == m { on = 1; next }
        /^mark / { on = 0 }
        on && $1 == "conf" {
            w = $3; h = $4
            if (w < 200 || h < 150 || (w < B[1] && w < A[1]) || (h < B[2] && h < A[2])) printf "%sx%s ", w, h
        }' "$L")
    _cur=$(size_now); _lc=$(last_conf)
    [ -z "$_bad" ] && [ "$_cur" = "$_after" ] \
        && pass "$_name: no transient tiny or wrong size ($_before -> $_after; sizes given: $(awk -v m="mark $_name" '$0 == m {on=1; next} /^mark /{on=0} on && $1 == "conf" {printf "%sx%s ", $3, $4}' "$L"))" \
        || fail "$_name: sizes given $_bad(want none under 200x150 or below both $_before and $_after), now $_cur (frame's client area $_after), last told $_lc"
}
client_size() {
    # the frame's client area: the program's window is sized to it by design
    sleep 1.5; size_now
}

probe activate "$W" "$N" >/dev/null; sleep 1
s0=$(size_now)

mark maximize; probe maximize "$W" "$N" >/dev/null; s1=$(client_size)
check_step maximize "$s0" "$s1"
[ "${s1%x*}" -gt 900 ] || fail "maximize: its size $s1 (want about the screen's width)"
mark restore; probe restore "$W" "$N" >/dev/null; s2=$(client_size)
check_step restore "$s1" "$s2"
[ "$s2" = "$s0" ] && pass "restored, it is the size it had ($s2)" || fail "restored: $s2, before maximize $s0"

mark ask-maximize; "$T/netwm-fullscreen" "$XID" 1 maximize; sleep 1; s3=$(client_size)
check_step ask-maximize "$s2" "$s3"
mark ask-restore; "$T/netwm-fullscreen" "$XID" 0 maximize; sleep 1; s4=$(client_size)
check_step ask-restore "$s3" "$s4"

mark minimize; probe minimize "$W" "$N" >/dev/null; sleep 1.5
mark unminimize; probe activate "$W" "$N" >/dev/null; s5=$(client_size)
# minimized it may keep its size or not be told at all; never a tiny one
check_step minimize "$s4" "$s4"
check_step unminimize "$s4" "$s5"
[ "$s5" = "$s4" ] && pass "minimized and restored, it is the size it had ($s5)" || fail "after minimize: $s5, before $s4"

mark maximized-minimize; probe maximize "$W" "$N" >/dev/null; sleep 1.5; s6=$(size_now)
probe minimize "$W" "$N" >/dev/null; sleep 1.5; probe activate "$W" "$N" >/dev/null; s7=$(client_size)
check_step maximized-minimize "$s6" "$s7"
[ "$s7" = "$s6" ] && pass "maximized, minimized and back: still maximized at $s7" || fail "maximized/minimized: $s7 (was $s6)"
probe restore "$W" "$N" >/dev/null; sleep 1.5

probe activate "$W" "$N" >/dev/null; sleep 0.5; sP=$(size_now)
mark snap; xdotool key super+Right; s8=$(client_size)
check_step snap "$sP" "$s8"
mark unsnap; xdotool key super+Left; s9=$(client_size)
check_step unsnap "$s8" "$s9"

mark desktop; probe activate "$W" "$N" >/dev/null; sleep 0.5
sA=$(size_now); xdotool key ctrl+super+Right; sleep 2; xdotool key ctrl+super+Left; sB=$(client_size)
check_step desktop "$sA" "$sB"
[ "$sB" = "$sA" ] && pass "another desktop and back: the size it had ($sB)" || fail "desktop switch: $sB (was $sA)"

[ -n "${KEEP:-}" ] && echo "log: $L"
[ $RC = 0 ] && echo "embedconf: all passed"
exit $RC
