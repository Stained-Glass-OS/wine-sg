#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A new display scale while programs run, the rest of it (1120-1123; David
# 2026-10-06: "top of the line DPI scaling across the board"). In the shell
# under Xvfb at 2736x1824 (a Surface Pro 7's), started at 100%, then
# Settings' change to 175% (LogPixels 168) and back:
#   1. maximized windows fill the work area the grown taskbar leaves
#      (1120): the taskbar takes its new height after the windows were laid
#      out at the scale, and a maximized one stayed under it (about 30 px;
#      here they hear of the change first: told before the broadcast);
#      Wine now tells every window of a new work area (SPI_SETWORKAREA) and a
#      maximized one fills it again. Back at 100%, they reach down to it.
#   2. a per-monitor aware program (v2, v1) has the new scale for what Wine
#      draws for it (1121): its system DPI, LOGPIXELSY, SM_CXICON, the menu
#      bar's font, a popup menu opened after the change -- 1.75 times; on
#      Windows the system scales those per monitor, Wine at the system DPI,
#      so they stayed 100%'s, tiny. A DPI-unaware program and one aware of
#      the system DPI keep theirs (Wine scales them).
#   3. the desktop's icons, their titles and their grid at 175%, crisp
#      (explorer follows, 1122): 84 px medium icons on a 1.75 times grid; back
#      to 48 at 100%
#   4. the pointer over a Windows program: the X cursor theme's size for the
#      new scale (1123), 24 px at 100%, larger at 175%, 24 again at 100%
#   5. File Explorer, open across the change, lays itself out again at it,
#      crisp (per-monitor v2, 1122): its window, the tree's rows and the
#      folder's list (the shell's view: its font, its icons made again at the
#      new size, its columns) 1.75 times; as before back at 100%
#   6. Notepad (ours, per-monitor v2) the same: the window, its tab strip,
#      its text and menus at the new scale
#   7. every program is told the display changed (WM_DISPLAYCHANGE, 1125):
#      Chrome laid itself out at the new DPI but drew at the old one until
#      it read the displays again
#   5. at 100% nothing changes: the windows' state as before the first change
#   8. an appbar docked while programs are busy docks at once (1126), and
#      the maximized window still moves beside it
#
#   WINE=/opt/wine-sg/bin/wine test/dpifollow-gate.sh
#   Mutants: SG_MUTANT_WORKAREA_NOT_TOLD, SG_MUTANT_WORKAREA_STALE_SERIAL
#   (win32u sysparams.c), SG_MUTANT_WORKAREA_NO_REFIT (win32u window.c),
#   SG_MUTANT_SCALE_NEGATIVE_UNSIGNED (server/user.h): 1 fails;
#   SG_MUTANT_SYSDPI_FIXED (win32u sysparams.c), SG_MUTANT_MENU_FONT_ONCE
#   (win32u menu.c): 2 fails; SG_MUTANT_DESKTOP_ICONS_FIXED (explorer
#   desktop.c): 3 fails; SG_MUTANT_CURSOR_FIXED (winex11 x11drv_main.c): 4;
#   SG_MUTANT_FE_SYSTEM_AWARE (explorer fileexplorer.c),
#   SG_MUTANT_DEFVIEW_DPI_IGNORED (shell32 shlview.c),
#   SG_MUTANT_SHELL_ICONS_FIXED (shell32 iconcache.c): 5 fails;
#   SG_MUTANT_NOTEPAD_DPI_IGNORED (notepad main.c): 6 fails;
#   SG_MUTANT_NO_DISPLAYCHANGE (win32u sysparams.c): 7 fails;
#   SG_MUTANT_WORKAREA_SYNC_BROADCAST (explorer appbar.c): 8 fails.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CC="${CC:-cc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool "$MINGW" "$CC"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -d /usr/share/icons/Adwaita/cursors ] || { echo "SKIP: no Adwaita cursor theme"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dpifollow.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export XCURSOR_THEME=Adwaita XCURSOR_SIZE=24
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/dpifollow-probe.c" -lgdi32 -lcomctl32 -lshell32 || { fail "probe did not build"; exit 1; }
"$CC" -O2 -o "$T/xcur" "$HERE/dpifollow-xcursor.c" -lX11 -lXfixes 2>/dev/null || { echo "SKIP: no libXfixes headers"; exit 77; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
W=2736 H=1824
Xvfb -displayfd 3 -screen 0 "${W}x${H}x24" -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 96 /f >/dev/null 2>&1
"$WINESERVER" -w
# a file on the desktop, so it has an icon of its own besides the shell's
desk=$("$WINE" winepath -u 'C:\users\'"$USER"'\Desktop' 2>/dev/null | tr -d '\r')
[ -n "$desk" ] && mkdir -p "$desk" && echo hello > "$desk/notes.txt"
export SG_DESKTOP_DUMP="$("$WINE" winepath -w "$T/desk" 2>/dev/null | tr -d '\r')"
"$WINE" explorer "/desktop=shell,${W}x$H" >/dev/null 2>&1 &
sleep 8
unset SG_DESKTOP_DUMP

# File Explorer and Notepad, open across the changes
"$WINE" explorer 'C:\windows' >/dev/null 2>&1 &
"$WINE" notepad 'C:\windows\win.ini' >/dev/null 2>&1 &
sleep 6

state() { grep '^state' "$T/$1.log" | tail -1; }
field() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
rect() { "$WINE" "$T/probe.exe" rect "$1" 2>/dev/null | tr -d '\r'; }
# the maximized window reaches down to the work area's bottom and the taskbar's top (within its border)
fits() {
    awk -v s="$1" 'BEGIN { n = split(s, f, " "); for (i = 1; i <= n; i++) { split(f[i], kv, "="); v[kv[1]] = kv[2] }
        split(v["rect"], r, ","); split(v["work"], w, ","); split(v["bar"], b, ",");
        exit !(v["max"] == 1 && w[4] == b[2] && r[4] - w[4] >= 0 && r[4] - w[4] <= 12 && r[2] - w[2] <= 0 && r[2] - w[2] >= -12) }'
}
ratio() { awk -v a="$1" -v b="$2" -v lo="$3" -v hi="$4" 'BEGIN { exit !(a > 0 && b / a >= lo && b / a <= hi) }'; }
desk() { sed -n 's/^metrics //p' "$T/desk" 2>/dev/null; }
cursor() { "$T/xcur" 2>/dev/null; }

"$WINE" "$T/probe.exe" win pmv2 100 100 400 300 'Z:'"$T/v2.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win pmv1 600 100 400 300 'Z:'"$T/v1.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win unaware 1100 100 400 300 'Z:'"$T/unaware.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win system 1600 100 400 300 'Z:'"$T/system.log" >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win pmv2 200 200 400 300 'Z:'"$T/v2max.log" max >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" win unaware 300 300 400 300 'Z:'"$T/unmax.log" maxtop >/dev/null 2>&1 &
sleep 8
# the pointer over the per-monitor v2 window's client area (a standard arrow)
"$WINE" "$T/probe.exe" rect 'dpifollow pmv2' >/dev/null 2>&1
xdotool mousemove 300 300 sleep 0.3 mousemove 302 302 2>/dev/null; sleep 1

# --- at 100% -------------------------------------------------------------------------
s_v2=$(state v2); s_un=$(state unaware)
r_v2max=$(rect 'dpifollow pmv2 max'); r_unmax=$(rect 'dpifollow unaware maxtop')
d1=$(desk); c1=$(cursor); f1=$("$WINE" "$T/probe.exe" fe 2>/dev/null | tr -d '\r')
n1=$("$WINE" "$T/probe.exe" app Notepad SgNotepadTabs 2>/dev/null | tr -d '\r')
fits "$r_v2max" && fits "$r_unmax" \
    && pass "at 100%: the maximized windows fill the work area ($r_v2max)" \
    || fail "at 100%: maximized v2 '$r_v2max', unaware '$r_unmax'"
echo "$s_v2" | grep -q 'sys=96 caps=96 win=96 icon=32' && echo "$s_un" | grep -q 'sys=96 caps=96 win=96 icon=32' \
    && pass "at 100%: per-monitor v2 '$s_v2', unaware '$s_un'" || fail "at 100%: v2 '$s_v2', unaware '$s_un'"
echo "$d1" | grep -q 'icon=48 .*dpi=96' && pass "at 100%: the desktop's icons $d1" || fail "at 100%: the desktop '$d1' (want 48 px icons)"
[ "$c1" = 24x24 ] && pass "at 100%: the pointer ${c1}" || fail "at 100%: the pointer '$c1' (want 24x24)"
"$WINE" "$T/probe.exe" popup 'dpifollow pmv2' >/dev/null 2>&1; sleep 2
p1=$(grep '^popup' "$T/v2.log" | tail -1)

# --- Settings' change to 175% ------------------------------------------------------------
# the maximized windows hear of it first, the taskbar grows after they are laid out
"$WINE" "$T/probe.exe" tell 'dpifollow unaware maxtop' 168 >/dev/null 2>&1
"$WINE" "$T/probe.exe" tell 'dpifollow pmv2 max' 168 >/dev/null 2>&1
"$WINE" "$T/probe.exe" set 168 >/dev/null 2>&1
sleep 5
xdotool mousemove 304 304 2>/dev/null; sleep 1
# 1. maximized windows and the grown taskbar
r_v2max=$(rect 'dpifollow pmv2 max'); r_unmax=$(rect 'dpifollow unaware maxtop')
fits "$r_v2max" && fits "$r_unmax" \
    && pass "at 175%: maximized per-monitor v2 and DPI-unaware windows end where the grown taskbar begins ($r_v2max; $r_unmax)" \
    || fail "at 175%: a maximized window not on the new work area: v2 '$r_v2max', unaware '$r_unmax' (want rect's bottom at the work area's, the bar's top)"
# 2. per-monitor aware: what Wine draws for it, at the new scale
s=$(state v2); s1=$(state v1)
p2=$(grep '^popup' "$T/v2.log" | tail -1)
"$WINE" "$T/probe.exe" popup 'dpifollow pmv2' >/dev/null 2>&1; sleep 2
p2=$(grep '^popup' "$T/v2.log" | tail -1)
ph1=$(echo "$p1" | sed 's/.*x//'); ph2=$(echo "$p2" | sed 's/.*x//')
if echo "$s" | grep -q 'sys=168 caps=168 win=168 icon=56' && ratio "$(field "$s_v2" item)" "$(field "$s" item)" 1.5 2.0 \
   && ratio "${ph1:-0}" "${ph2:-0}" 1.35 2.0 && echo "$s1" | grep -q 'sys=168 caps=168 win=168 icon=56'; then
    pass "per-monitor aware at 175%: v2 '$s' (menu bar item $(field "$s_v2" item) -> $(field "$s" item) px), popup menu ${p1#popup } -> ${p2#popup }; v1 '$s1'"
else
    fail "per-monitor aware at 175%: v2 '$s' (was '$s_v2'), popup '${p1:-none}' -> '${p2:-none}', v1 '$s1' (want sys=168 caps=168 icon=56, menus 1.75 times)"
fi
s=$(state unaware); ss=$(state system)
echo "$s" | grep -q 'sys=96 caps=96 win=96 icon=32' && echo "$ss" | grep -q 'sys=96 caps=96 win=96 icon=32' \
    && pass "DPI unaware and system aware at 175%: their own DPI kept ('$s'; '$ss'), Wine scales them" \
    || fail "unaware/system at 175%: '$s'; '$ss' (want 96 kept)"
# 7. each program told the display changed (1125): Chromium reads the
# displays' scale again only then
dc=0; for n in v2 unaware system; do grep -q '^displaychange 2736x1824' "$T/$n.log" && dc=$((dc + 1)); done
[ $dc = 3 ] && pass "per-monitor v2, unaware and system-aware programs are told the display changed (WM_DISPLAYCHANGE)" \
    || fail "WM_DISPLAYCHANGE reached $dc of 3 programs"
# 3. the desktop's icons
d2=$(desk)
if echo "$d2" | grep -q 'icon=84 .*dpi=168' && ratio "$(field "$d1" grid)" "$(field "$d2" grid)" 1.5 2.0; then
    pass "the desktop's icons at 175%: $d1 -> $d2"
else
    fail "the desktop's icons at 175%: '$d1' -> '$d2' (want 84 px icons, the grid 1.75 times)"
fi
# 5. File Explorer, per-monitor v2, laid out again: its window, its tree's
# rows, the folder's list (the shell's view: font, icons) 1.75 times
f2=$("$WINE" "$T/probe.exe" fe 2>/dev/null | tr -d '\r')
fv() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
if [ "$(fv "$f2" pmv2)" = 1 ] && ratio "$(fv "$f1" size | cut -dx -f1)" "$(fv "$f2" size | cut -dx -f1)" 1.70 1.80 \
   && ratio "$(fv "$f1" tree)" "$(fv "$f2" tree)" 1.6 1.9 && ratio "$(fv "$f1" list | cut -d, -f2)" "$(fv "$f2" list | cut -d, -f2)" 1.5 2.0; then
    pass "File Explorer at 175%, laid out again (per-monitor v2): $f1 -> $f2"
else
    fail "File Explorer at 175%: '$f1' -> '$f2' (want pmv2=1, the window, the tree's rows and the list's 1.75 times)"
fi
# 6. Notepad (ours), per-monitor v2: laid out again, its tab strip 1.75 times
n2=$("$WINE" "$T/probe.exe" app Notepad SgNotepadTabs 2>/dev/null | tr -d '\r')
if [ "$(fv "$n2" pmv2)" = 1 ] && ratio "$(fv "$n1" size | cut -dx -f1)" "$(fv "$n2" size | cut -dx -f1)" 1.70 1.80 \
   && ratio "$(fv "$n1" child | cut -dx -f2)" "$(fv "$n2" child | cut -dx -f2)" 1.70 1.80; then
    pass "Notepad at 175%, laid out again (per-monitor v2): $n1 -> $n2"
else
    fail "Notepad at 175%: '$n1' -> '$n2' (want pmv2=1, the window and its tab strip 1.75 times)"
fi
# 4. the pointer
c2=$(cursor)
w2=${c2%x*}
[ "${w2:-0}" -ge 36 ] 2>/dev/null && pass "the pointer at 175%: $c1 -> $c2 (the theme's size for the scale)" \
    || fail "the pointer at 175%: $c1 -> '$c2' (want at least 36 px)"

# --- back to 100% -------------------------------------------------------------------------
"$WINE" "$T/probe.exe" tell 'dpifollow unaware maxtop' 96 >/dev/null 2>&1
"$WINE" "$T/probe.exe" tell 'dpifollow pmv2 max' 96 >/dev/null 2>&1
"$WINE" "$T/probe.exe" set 96 >/dev/null 2>&1
sleep 5
xdotool mousemove 300 300 2>/dev/null; sleep 1
r_v2max=$(rect 'dpifollow pmv2 max'); r_unmax=$(rect 'dpifollow unaware maxtop')
fits "$r_v2max" && fits "$r_unmax" && pass "back at 100%: the maximized windows reach down to the 40 px taskbar again ($r_v2max)" \
    || fail "back at 100%: maximized v2 '$r_v2max', unaware '$r_unmax'"
s=$(state v2); d3=$(desk); c3=$(cursor); f3=$("$WINE" "$T/probe.exe" fe 2>/dev/null | tr -d '\r')
[ "$(fv "$f3" tree)" = "$(fv "$f1" tree)" ] && [ "$(fv "$f3" list)" = "$(fv "$f1" list)" ] \
    && pass "back at 100%: File Explorer as before ($f3)" || fail "back at 100%: File Explorer '$f3' (was '$f1')"
n3=$("$WINE" "$T/probe.exe" app Notepad SgNotepadTabs 2>/dev/null | tr -d '\r')
[ "$(fv "$n3" child | cut -dx -f2)" = "$(fv "$n1" child | cut -dx -f2)" ] \
    && pass "back at 100%: Notepad as before ($n3)" || fail "back at 100%: Notepad '$n3' (was '$n1')"
[ "$(echo "$s" | sed 's/ item=.*//')" = "$(echo "$s_v2" | sed 's/ item=.*//')" ] && [ "$(field "$s" item)" = "$(field "$s_v2" item)" ] \
    && pass "back at 100%: per-monitor v2 as before the change ('$s')" || fail "back at 100%: v2 '$s' (was '$s_v2')"
[ "$d3" = "$d1" ] && pass "back at 100%: the desktop's icons as before ($d3)" || fail "back at 100%: the desktop '$d3' (was '$d1')"
[ "$c3" = 24x24 ] && pass "back at 100%: the pointer $c3" || fail "back at 100%: the pointer '$c3' (want 24x24)"

# 8. an appbar docked while a program is at work (its window not answering)
# docks at once (1126): explorer told every window of the new work area
# inside the appbar's own request, waiting up to 2 s for each busy window
# (a docked touch keyboard took ~4.5 s); a thread of its own tells them now.
# The maximized window still hears of it and moves right of the appbar.
"$WINE" "$T/probe.exe" busy >/dev/null 2>&1 &
"$WINE" "$T/probe.exe" busy >/dev/null 2>&1 &
sleep 3
"$WINE" "$T/probe.exe" appbar 300 8 > "$T/appbar.out" 2>/dev/null &
sleep 6
ab=$(tr -d '\r' < "$T/appbar.out"); r_ab=$(rect 'dpifollow pmv2 max')
ms=$(echo "$ab" | sed -n 's/^appbar ms=//p')
[ -n "$ms" ] && [ "$ms" -lt 1000 ] && pass "an appbar docks at once beside busy programs ($ab)" \
    || fail "an appbar dock beside busy programs: '$ab' (want under 1000 ms)"
echo "$r_ab" | awk '{ for (i = 1; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
    split(v["rect"], r, ","); split(v["work"], w, ",");
    exit !(v["max"] == 1 && w[1] == 300 && r[1] - w[1] <= 0 && r[1] - w[1] >= -12) }' \
    && pass "the maximized window moves right of the docked appbar ($r_ab)" \
    || fail "with the appbar docked: '$r_ab' (want the work area from x=300, the maximized window on it)"
sleep 4

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
