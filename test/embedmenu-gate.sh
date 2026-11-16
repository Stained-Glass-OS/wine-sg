#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A framed Linux program's menus stay open (wine-sg 7800; David 2026-11-15:
# Steam's View, Friends, Games and Help menus showed "for only a single frame
# and then disappear"). Steam's menus are windows of their own that close
# when they lose the X focus. A window manager never frames such a window --
# a menu (_NET_WM_WINDOW_TYPE_*MENU, a tooltip, a combo box's list...), or a
# window transient for its program's window that asks for no decorations
# and no taskbar button -- and gives it no taskbar button. Ours framed it as
# soon as the compositor listed it (a transient window is listed once its
# main window, framed, is no longer a top-level the compositor shows):
# reparented into a Wine window, the menu lost the focus and closed.
#
# A stand-in program (embedmenu-app.c), its main window framed by the
# taskbar (0762), opens a menu window under a click; the compositor's list
# then names the menu too, as sg-compositor's does. For each kind of menu
# window:
#   transient  WM_TRANSIENT_FOR, no title, no decorations, skip-taskbar
#   menu       _NET_WM_WINDOW_TYPE_POPUP_MENU
#   or         override-redirect, pointer and keyboard grabbed
#   plain      no title, no decorations, nothing else (a normal window as
#              far as anyone can tell): it is framed -- the gate checks that
#              this rule is not "frame nothing new"
# checked: three seconds later (several taskbar ticks) the menu is open, a
# top-level, shown in front (its colour where it is), with the X focus (or
# its grab); no Wine window holds it; the taskbar has no button for it; the
# shell did not put its desktop in front (XDESKTOP); the click handled by
# its frame only then (managed menus), it keeps the focus. Escape then closes
# it. At the end the shell is still running: a window gone as it was framed
# (the plain one) ended it (BadWindow).
#
#   WINE=/opt/wine-sg/bin/wine test/embedmenu-gate.sh
# Mutants (dlls/winex11.drv/sg_embed.c): SG_MUTANT_EMBED_FRAMES_MENUS
# (menus framed as before) fails transient and menu; SG_MUTANT_EMBED_FOCUS_OVER_MENU
# (the frame takes the focus back from the menu) fails "handled late";
# SG_MUTANT_EMBED_ERROR_UNSYNCED fails "the shell is still running".
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xwininfo import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedmenu.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/menuapp" "$HERE/embedmenu-app.c" -lX11 || { echo "FAIL  embedmenu-app did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${EMBEDMENU_DISPLAY:-}" ]; then
    Xvfb ":$EMBEDMENU_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$EMBEDMENU_DISPLAY" > "$T/display"; sleep 1
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
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" SG_FAKE_FOCUS=1 XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 & EXPID=$!
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"

parent_of() { xwininfo -id "$1" -tree 2>/dev/null | awk '/Parent window id:/ {print $4; exit}'; }
root_id=$(xwininfo -root | awk '/Window id:/ {print $4; exit}')
framed() {
    _k=0
    while [ $_k -lt 80 ]; do
        _p=$(parent_of "$1"); [ -n "$_p" ] && [ "$_p" != "$root_id" ] && return 0
        sleep 0.25; _k=$((_k + 1))
    done
    return 1
}
logval() { awk -v k="$2" '$1 == k {print $2}' "$1" | tail -1; }
# the menu's colour where it is (embedmenu-app's 0xd02020)
menu_px() {
    _g=$(xwininfo -id "$1" 2>/dev/null)
    _x=$(echo "$_g" | awk '/Absolute upper-left X:/ {print $4}'); _y=$(echo "$_g" | awk '/Absolute upper-left Y:/ {print $4}')
    [ -n "$_x" ] || { echo none; return; }
    import -window root -crop "1x1+$((_x + 80))+$((_y + 60))" -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'
}
buttons() { "$WINE" probe.exe list SgLinuxWindow - 2>/dev/null | tr -d '\r' | grep -c .; }

for MODE in transient menu or plain; do
    L="$T/app-$MODE.log"; : > "$L"
    "$T/menuapp" "$MODE" "$L" & AP=$!
    i=0; while [ -z "$(logval "$L" main)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    MAIN=$(logval "$L" main)
    sleep 0.5
    printf '%d shown - MenuTest\tMenu Test\nEND\n' "$MAIN" > "$T/list"
    if ! framed "$MAIN"; then fail "$MODE: the program's main window was not framed"; kill "$AP"; AP=; continue; fi
    sleep 1.5
    nb=$(buttons)
    : > "$T/commands"
    g=$(xwininfo -id "$MAIN")
    mx=$(echo "$g" | awk '/Absolute upper-left X:/ {print $4}'); my=$(echo "$g" | awk '/Absolute upper-left Y:/ {print $4}')
    xdotool mousemove $((mx + 60)) $((my + 40)); sleep 0.3; xdotool click 1
    i=0; while [ -z "$(logval "$L" open)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    MENU=$(logval "$L" open)
    [ -n "$MENU" ] || { fail "$MODE: the click opened no menu (the click did not reach the program)"; kill "$AP"; AP=; continue; }
    # the compositor lists it (sg-compositor's program_surface: transient for
    # a window that is not a top-level it shows, or no transient at all); an
    # override-redirect window it never lists
    [ "$MODE" = or ] || printf '%d shown - MenuTest\tMenu Test\n%d shown focused MenuTest\t\nEND\n' "$MAIN" "$MENU" > "$T/list"
    sleep 3
    closed=$(awk '$1 == "close" {print $2}' "$L" | head -1)
    par=$(parent_of "$MENU")
    fx=$(xdotool getwindowfocus 2>/dev/null)
    px=$(menu_px "$MENU")
    nb2=$(buttons)
    xd=$(grep -c XDESKTOP "$T/commands" 2>/dev/null)
    if [ "$MODE" = plain ]; then
        # a window nothing marks as a menu is the taskbar's to frame, as before
        # (framed, it lost the focus as it was reparented: the stand-in closed it)
        if [ -n "$par" ] && [ "$par" != "$root_id" ]; then pass "plain: a window not marked as a menu is still framed (in $par)"
        elif [ -n "$closed" ] && [ "$closed" != escape ]; then pass "plain: a window not marked as a menu is still framed (and closed as it was: $closed)"
        else fail "plain: an unmarked window was not framed (parent $par, closed '$closed'): the rule frames too little"; fi
    else
        [ -z "$closed" ] && [ "$par" = "$root_id" ] && [ "$px" = "#D02020" ] && [ "$nb2" = "$nb" ] && [ "${xd:-0}" = 0 ] \
            && { [ "$MODE" = or ] || [ "$fx" = "$MENU" ]; } \
            && pass "$MODE: the menu stays open, a top-level shown in front ($px)$([ "$MODE" = or ] || echo ", with the focus"), no frame or button for it" \
            || fail "$MODE: menu $(if [ -n "$closed" ]; then echo "closed ($closed)"; else echo "open"; fi), parent $par (root $root_id), colour $px (want #D02020), focus $fx (menu $MENU), stand-ins $nb -> $nb2, XDESKTOP $xd"
        # the click into the program that opened it, handled by its frame only
        # now (winex11's WM_X11DRV_SG_EMBED_ACTIVATE, posted by the click: a
        # busy shell, or a menu quicker than it): the menu keeps the focus
        if [ "$MODE" != or ] && [ -z "$closed" ]; then
            "$WINE" probe.exe post SgLinuxWindow "Menu Test" 0x80001007 0 >/dev/null 2>&1; sleep 1
            c2=$(awk '$1 == "close" {print $2}' "$L" | head -1); fx2=$(xdotool getwindowfocus 2>/dev/null)
            [ -z "$c2" ] && [ "$fx2" = "$MENU" ] && pass "$MODE: the click handled late by its frame, the menu keeps the focus" \
                || fail "$MODE: the click handled late by its frame: menu $(if [ -n "$c2" ]; then echo "closed ($c2)"; else echo open; fi), focus $fx2 (menu $MENU)"
        fi
        # Escape closes it (it has the keys)
        xdotool key Escape; sleep 0.8
        [ "$MODE" = or ] || [ "$(awk '$1 == "close" {print $2}' "$L" | tail -1)" = escape ] || [ -n "$closed" ] \
            || fail "$MODE: Escape did not reach the menu"
    fi
    # the program goes: its frame closes it
    "$WINE" probe.exe wmclose SgLinuxWindow "Menu Test" >/dev/null 2>&1
    i=0; while kill -0 "$AP" 2>/dev/null && [ $i -lt 30 ]; do sleep 0.2; i=$((i + 1)); done
    kill "$AP" 2>/dev/null; AP=
    printf 'END\n' > "$T/list"
    sleep 1.5
done
"$WINE" probe.exe exists Shell_TrayWnd "" 2>/dev/null | grep -q 'exists=1' && kill -0 "$EXPID" 2>/dev/null \
    && pass "the shell is still running" || fail "the shell ended: $(grep -m1 -A1 'X Error' "$T/explorer.out" | tr '\n' ' ')"
[ $RC = 0 ] && echo "embedmenu: all passed"
exit $RC
