# The steps of desktop-gate.sh, run inside its Xvfb session (sourced: T, WINE, DESK,
# PUB and DUMP are set). Each check appends PASS or FAIL to $T/results.txt.
R="$T/results.txt"; : > "$R"
pass() { printf 'PASS  %s\n' "$*" >> "$R"; }
fail() { printf 'FAIL  %s\n' "$*" >> "$R"; }
shot() { import -window root "$T/$1.png" 2>/dev/null; }
probe() { "$WINE" vdesk-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
# the dump is written in text mode: CR LF
dump() { { tr -d '\r' < "$DUMP"; } 2>/dev/null; }
head1() { dump | head -1; }
field() { head1 | tr ' ' '\n' | sed -n "s/^$1=//p"; }
# the index and centre of the icon titled $1
item() { dump | grep " $1\$" | head -1 | cut -d' ' -f2,3; }
at() { set -- $(item "$1"); [ $# -eq 2 ] && echo "$2" | tr ',' ' '; }
index_of() { set -- $(item "$1"); echo "${1:--9}"; }
wait_for() {   # a command that succeeds, within $1 seconds
    t=$1; shift; i=0
    while ! "$@" && [ $i -lt $((t * 4)) ]; do sleep 0.25; i=$((i + 1)); done; "$@"
}
has_item() { dump | grep -q " $1\$"; }
field_is() { [ "$(field "$1")" = "$2" ]; }
selected_is() { [ "$(field selected)" = "$(index_of "$1")" ]; }
menu_open() { [ "$(probe find '#32768')" = "found=1" ]; }
visible() { [ "$(probe find "$@")" = "found=1" ]; }
gone() { ! visible "$@"; }
EMPTY="900 150"
empty_menu() { xdotool mousemove $EMPTY click 3; wait_for 10 menu_open; }
empty_click() { xdotool mousemove $EMPTY click 1; sleep 1; }
exists() { [ -e "$1" ]; }

cd "$(dirname "$DUMP")" || exit 1
export SG_DESKTOP_DUMP='C:\desk.txt'
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.txt" 2>&1 &
sleep 6
# a program keeps the session going; the desktop is drawn once a window has been on it
"$WINE" cmd /c "ping -n 2000 127.0.0.1 >nul" >/dev/null 2>&1 &
SESSION=$!
"$WINE" vdesk-probe.exe window Away 600 380 >/dev/null 2>&1 &
wait_for 30 has_item before.txt
cp "$DUMP" "$T/dump-start.txt"

# --- B15: the Desktop folders are watched --------------------------------------------------
has_item before.txt && pass "a file in the Desktop folder before sign-in is on the desktop" || fail "no before.txt: $(cat "$DUMP" 2>/dev/null)"
mkdir "$DESK/After Folder"; echo x > "$DESK/after.txt"; echo x > "$PUB/public.txt"
if wait_for 15 has_item after.txt && wait_for 5 has_item "After Folder" && wait_for 5 has_item public.txt; then
    pass "files and a folder made after sign-in (the user's and the public Desktop) appear by themselves"
else fail "new items not shown: $(dump | cut -d' ' -f5- | tr '\n' '|')"; fi
rm "$DESK/after.txt"
wait_for 15 eval '! has_item after.txt' && pass "a file deleted from the Desktop folder goes away" || fail "after.txt still shown"
cp "$DUMP" "$T/dump-b15.txt"

# --- B16: selection -----------------------------------------------------------------------------
shot before-select
set -- $(at before.txt)
if [ $# -eq 2 ]; then
    X=$1; Y=$2
    xdotool mousemove "$X" "$Y" click 1; sleep 1
    wait_for 5 selected_is before.txt && pass "a click selects the icon" || fail "selected=$(field selected), before.txt is $(index_of before.txt)"
    shot selected
    lit=$(python3 - "$T/before-select.png" "$T/selected.png" "$X" "$Y" <<'PY'
import sys
from PIL import Image
a = Image.open(sys.argv[1]).convert('RGB'); b = Image.open(sys.argv[2]).convert('RGB')
x, y = int(sys.argv[3]), int(sys.argv[4])
print(sum(1 for i in range(x - 34, x + 34) for j in range(y - 30, y + 30) if a.getpixel((i, j)) != b.getpixel((i, j))))
PY
)
    [ "${lit:-0}" -gt 400 ] && pass "the selected icon is drawn highlighted ($lit pixels changed)" || fail "no highlight ($lit pixels changed)"
else fail "before.txt has no place: $(dump | tr '\n' '|')"; fi
empty_click
wait_for 5 field_is selected -1 && pass "a click on the empty desktop clears the selection" || fail "selected=$(field selected)"

# --- B15: F5 ------------------------------------------------------------------------------------
"$WINE" reg add 'HKCU\Software\Stained Glass\Desktop' /v SortBy /t REG_DWORD /d 1 /f >/dev/null 2>&1
sleep 1; empty_click; xdotool key F5
wait_for 10 field_is sort 1 && pass "F5 reads the desktop again" || fail "F5: $(head1)"
"$WINE" reg add 'HKCU\Software\Stained Glass\Desktop' /v SortBy /t REG_DWORD /d 0 /f >/dev/null 2>&1
empty_click; xdotool key F5; wait_for 10 field_is sort 0

# --- B16: the desktop's menu ------------------------------------------------------------------------
if empty_menu; then pass "right-clicking the empty desktop opens a menu"; else fail "no menu on the desktop"; fi
shot menu
xdotool key w; sleep 1; shot menu-new; xdotool key f
wait_for 10 exists "$DESK/New folder" && [ -d "$DESK/New folder" ] && pass "New > Folder makes 'New folder'" || fail "no New folder: $(ls "$DESK" | tr '\n' '|')"
wait_for 10 selected_is "New folder" && pass "... and selects it" || fail "New folder not selected: $(head1)"
empty_menu; xdotool key w t
wait_for 10 exists "$DESK/New Text Document.txt" && pass "New > Text Document makes 'New Text Document.txt'" || fail "no text document: $(ls "$DESK" | tr '\n' '|')"
wait_for 10 selected_is "New Text Document.txt" && pass "... and selects it" || fail "text document not selected: $(head1)"
empty_menu; xdotool key w f
wait_for 10 exists "$DESK/New folder (2)" && pass "a second New > Folder makes 'New folder (2)'" || fail "no New folder (2): $(ls "$DESK" | tr '\n' '|')"

empty_menu; xdotool key v l
wait_for 10 field_is size 48 && pass "View > Large icons" || fail "large icons: $(head1)"
sleep 1; shot large
empty_menu; xdotool key v m
wait_for 10 field_is size 32 && pass "View > Medium icons" || fail "medium icons: $(head1)"
empty_menu; xdotool key o d
wait_for 10 field_is sort 3 && pass "Sort by > Date modified" || fail "sort: $(head1)"
reg_sort=$("$WINE" reg query 'HKCU\Software\Stained Glass\Desktop' /v SortBy 2>/dev/null | tr -d '\r' | awk '/SortBy/ {print $3}')
[ "$reg_sort" = 0x3 ] && pass "... kept in the registry" || fail "SortBy in the registry: $reg_sort"
empty_menu; xdotool key v d
wait_for 10 field_is hidden 1 && pass "View > Show desktop icons hides them" || fail "hide: $(head1)"
sleep 1; shot hidden
empty_menu; xdotool key v d
wait_for 10 field_is hidden 0 && pass "... and shows them again" || fail "show: $(head1)"
sleep 1; shot shown
has_settings() { grep -q "$1" "$(dirname "$DUMP")/settings.txt" 2>/dev/null; }
empty_menu; xdotool key d
wait_for 20 has_settings ms-settings:display && pass "Display settings opens ms-settings:display" || fail "display settings: $(cat settings.txt 2>/dev/null)"
empty_menu; xdotool key r
wait_for 20 has_settings ms-settings:personalization && pass "Personalize opens ms-settings:personalization" || fail "personalize: $(cat settings.txt 2>/dev/null)"

# --- B16: an icon's own menu -------------------------------------------------------------------------
set -- $(at before.txt)
if [ $# -eq 2 ]; then
    xdotool mousemove "$1" "$2" click 3
    wait_for 10 menu_open && pass "right-clicking an icon opens a menu" || fail "no menu on the icon"
    sleep 1; shot item-menu; dump > "$T/dump-item-menu.txt"
    wait_for 5 selected_is before.txt && pass "... and selects it first" || fail "the right-clicked icon is not selected"
    xdotool key o
    wait_for 30 visible Notepad && pass "its shell menu's Open opens it (Notepad)" || fail "Open did not open Notepad"
fi

# --- B18: Alt+F4 -------------------------------------------------------------------------------------------
sleep 2
xdotool key alt+F4
wait_for 15 gone Notepad && pass "Alt+F4 closes Notepad" || { shot notepad-altf4; fail "Notepad still open after Alt+F4"; }
sleep 1; empty_click; xdotool key alt+F4
wait_for 10 visible '#32770' 'Shut Down' && pass "Alt+F4 on the desktop asks (Shut Down)" || fail "no Shut Down dialog"
shot shutdown
xdotool key Escape
wait_for 10 gone '#32770' 'Shut Down' && pass "Escape closes the dialog" || fail "the dialog stayed"
sleep 2
kill -0 $SESSION 2>/dev/null && visible Shell_TrayWnd && pass "the session goes on, the taskbar shown" || fail "the session ended or the taskbar went"
# after the last window closes the taskbar is the foreground window: Alt+F4 there
# asks too (it used to hide the taskbar for good)
xdotool mousemove 946 396 click 1; wait_for 10 gone SgVdeskProbe Away
"$WINE" notepad >/dev/null 2>&1 &
wait_for 30 visible Notepad; sleep 2
xdotool key alt+F4; wait_for 15 gone Notepad; sleep 2
xdotool key alt+F4
wait_for 10 visible '#32770' 'Shut Down' && pass "Alt+F4 after the last window closed asks" || fail "no dialog after the last window closed"
shot taskbar-altf4
xdotool key Escape; sleep 2
kill -0 $SESSION 2>/dev/null && visible Shell_TrayWnd && pass "the taskbar is still there" || fail "the taskbar was hidden or the session ended"
set -- $(at "This PC")
if [ $# -eq 2 ]; then
    xdotool mousemove "$1" "$2" click --repeat 2 --delay 80 1
    wait_for 30 visible ExplorerWClass && sleep 2
    visible ExplorerWClass && pass "This PC opens File Explorer" || fail "no File Explorer"
    shot explorer
    xdotool key alt+F4
    wait_for 15 gone ExplorerWClass && pass "Alt+F4 closes File Explorer" || fail "File Explorer still open after Alt+F4"
else fail "no This PC icon"; fi
cp "$DUMP" "$T/dump-end.txt"
shot end
# last, as it ends the session: Restart in the dialog, OK
has_power() { grep -q "$1" "$T/power.txt" 2>/dev/null; }
empty_click; xdotool key alt+F4
if wait_for 10 visible '#32770' 'Shut Down'; then
    xdotool key Up; sleep 0.5; shot restart; xdotool key Return
    wait_for 30 has_power 'shutdown reboot' && pass "OK on Restart restarts (the power helper is asked: $(cat "$T/power.txt"))" \
        || fail "Restart: the power helper was asked '$(cat "$T/power.txt" 2>/dev/null)'"
else fail "no dialog for Restart"; fi
