# The steps of drag-gate.sh, run inside its Xvfb session (sourced: T, WINE, DESK,
# DOCS, MUSIC, U and DUMP are set). Each check appends PASS or FAIL to $T/results.txt.
R="$T/results.txt"; : > "$R"
pass() { printf 'PASS  %s\n' "$*" >> "$R"; }
fail() { printf 'FAIL  %s\n' "$*" >> "$R"; }
check() { msg=$1; shift; if "$@"; then pass "$msg"; else fail "$msg"; fi; }
shot() { import -window root "$T/$1.png" 2>/dev/null; }
vprobe() { "$WINE" vdesk-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
dprobe() { "$WINE" drag-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
wait_for() {   # a command that succeeds, within $1 seconds
    t=$1; shift; i=0
    while ! "$@" && [ $i -lt $((t * 4)) ]; do sleep 0.25; i=$((i + 1)); done; "$@"
}
# the desktop's dump: its first line's fields, each icon's index, centre and title
dump() { { tr -d '\r' < "$DUMP"; } 2>/dev/null; }
field() { dump | head -1 | tr ' ' '\n' | sed -n "s/^$1=//p"; }
field_is() { [ "$(field "$1")" = "$2" ]; }
item() { dump | grep "^item .* $1\$" | head -1 | cut -d' ' -f2,3; }
at() { set -- $(item "$1"); [ $# -eq 2 ] && echo "$2" | tr ',' ' '; }
idx() { set -- $(item "$1"); echo "${1:--9}"; }
has_item() { dump | grep -q "^item .* $1\$"; }
selected() { field sel | tr ',' '\n' | grep -qx "$(idx "$1")"; }
# a drag with the left button, in steps, the mouse held at the end for $5 (a command)
drag() {
    xdotool mousemove "$1" "$2" mousedown 1; sleep 0.3
    xdotool mousemove $(( $1 + ($3 - $1) / 8 )) $(( $2 + ($4 - $2) / 8 )); sleep 0.3
    xdotool mousemove $(( $1 + ($3 - $1) / 2 )) $(( $2 + ($4 - $2) / 2 )); sleep 0.3
    xdotool mousemove "$3" "$4"; sleep 0.3; xdotool mousemove $(( $3 + 1 )) $(( $4 + 1 )); sleep 0.6
    [ $# -ge 5 ] && eval "$5"
    xdotool mouseup 1; sleep 1
}
pixel() { python3 -c "
from PIL import Image
print('%02x%02x%02x' % Image.open('$T/$1.png').convert('RGB').getpixel(($2, $3)))"; }
near() { [ $(( $1 > $3 ? $1 - $3 : $3 - $1 )) -le "$5" ] && [ $(( $2 > $4 ? $2 - $4 : $4 - $2 )) -le "$5" ]; }
lv() { dprobe lv "$1"; }
lv_item() { lv "$1" | grep " $2\$" | head -1 | cut -d' ' -f3 | tr ',' ' '; }
lv_state() { lv "$1" | grep " $2\$" | head -1 | cut -d' ' -f4; }
lv_count() { lv "$1" | head -1 | tr ' ' '\n' | sed -n "s/^$2=//p"; }
title_is() { [ "$("$WINE" explorer-probe.exe title 2>/dev/null | tr -d '\r')" = "title=$1" ]; }

cd "$(dirname "$DUMP")" || exit 1
export SG_DESKTOP_DUMP='C:\desk.txt'
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.txt" 2>&1 &
sleep 6
"$WINE" cmd /c "ping -n 3000 127.0.0.1 >nul" >/dev/null 2>&1 &
"$WINE" vdesk-probe.exe window Away 600 380 >/dev/null 2>&1 &
wait_for 30 has_item d.txt
cp "$DUMP" "$T/dump-start.txt"

# --- the desktop: a rubber band ------------------------------------------------------------------
set -- $(at a.txt); AX=$1; AY=$2
set -- $(at b.txt); BY=$2
shot band-before
# from the empty desktop right of a.txt's top, down and left past b.txt's middle
drag $((AX + 140)) $((AY - 25)) 20 "$BY" 'dump > "$T/dump-band.txt"; shot band'
grep -q ' band=1' "$T/dump-band.txt" && pass "a press on the empty desktop and a drag draws a rubber band" \
    || fail "no band while dragging: $(head -1 "$T/dump-band.txt")"
before=$(pixel band-before $((AX + 100)) "$AY"); during=$(pixel band $((AX + 100)) "$AY")
[ "$before" != "$during" ] && pass "... filled, translucent ($before -> $during)" || fail "the band is not drawn ($before)"
[ "$(field selcount)" = 2 ] && selected a.txt && selected b.txt && pass "the icons the band touches are selected (a.txt, b.txt)" \
    || fail "band selection: $(dump | head -1)"
field_is band "" && [ "$(pixel band-before $((AX + 100)) "$AY")" = "$(shot band-after; pixel band-after $((AX + 100)) "$AY")" ] \
    && pass "... and the band goes when the button is let go" || fail "the band stayed: $(dump | head -1)"

# --- Ctrl+click, Shift+click -------------------------------------------------------------------------
set -- $(at c.txt); CX=$1; CY=$2
xdotool mousemove "$CX" "$CY" keydown ctrl click 1 keyup ctrl; sleep 0.8
[ "$(field selcount)" = 3 ] && selected c.txt && pass "Ctrl+click adds an icon to the selection" || fail "Ctrl+click: $(dump | head -1)"
xdotool mousemove "$AX" "$AY" keydown ctrl click 1 keyup ctrl; sleep 0.8
[ "$(field selcount)" = 2 ] && ! selected a.txt && pass "Ctrl+click on a selected icon takes it out" || fail "Ctrl+click off: $(dump | head -1)"
set -- $(at b.txt); BX=$1
set -- $(at d.txt); DX=$1; DY=$2
xdotool mousemove "$BX" "$BY" click 1; sleep 0.8
[ "$(field selcount)" = 1 ] && selected b.txt && pass "a click on a selected icon selects it alone" || fail "click: $(dump | head -1)"
xdotool mousemove "$DX" "$DY" keydown shift click 1 keyup shift; sleep 0.8
[ "$(field selcount)" = 3 ] && selected b.txt && selected c.txt && selected d.txt \
    && pass "Shift+click selects from the last icon clicked to this one" || fail "Shift+click: $(dump | head -1)"

# --- the selection's clipboard and menu -------------------------------------------------------------
xdotool key ctrl+c
has_clip3() { [ "$(vprobe clip | grep -c '^file=')" = 3 ]; }
wait_for 10 has_clip3 && pass "Ctrl+C puts every selected file on the clipboard" || fail "clipboard: $(vprobe clip | tr '\n' ' ')"
xdotool mousemove "$CX" "$CY" click 3
menu_open() { [ "$(vprobe find '#32768')" = "found=1" ]; }
if wait_for 10 menu_open; then
    [ "$(field selcount)" = 3 ] && pass "right-clicking a selected icon keeps the selection for its menu" || fail "menu: $(dump | head -1)"
    shot menu; xdotool key Escape; sleep 0.5
else fail "no menu on a selected icon"; fi

# --- dragging icons on the desktop ---------------------------------------------------------------------
xdotool mousemove 900 150 click 1; sleep 0.5
drag "$DX" "$DY" 500 150
set -- $(at d.txt)
[ $# -eq 2 ] && near "$1" "$2" 500 150 50 && pass "an icon dragged moves to where it is dropped ($1,$2)" || fail "d.txt is at $*"
dmoved="$1 $2"
"$WINE" reg query 'HKCU\Software\Stained Glass\Desktop\IconPositions' 2>/dev/null | grep -q 'd\.txt' \
    && pass "... and its place is kept (IconPositions)" || fail "no IconPositions entry"
xdotool key F5; sleep 2
[ "$(at d.txt)" = "$dmoved" ] && pass "... it stays there after F5" || fail "after F5 d.txt is at $(at d.txt)"
xdotool mousemove "$BX" "$BY" click 1; sleep 0.3
xdotool mousemove "$CX" "$CY" keydown ctrl click 1 keyup ctrl; sleep 0.5
drag "$BX" "$BY" 700 200 'shot dragging'
set -- $(at b.txt) $(at c.txt)
[ $# -eq 4 ] && near "$1" "$2" 700 200 50 && [ "$3" = "$1" ] && [ $(( $4 - $2 )) -gt 0 ] \
    && pass "two selected icons dragged move together, as they were ($1,$2 and $3,$4)" || fail "b.txt, c.txt at $*"
set -- $(at a.txt); AX=$1; AY=$2
set -- $(at Box); BOXX=$1; BOXY=$2
# (no click first: a press right after a click on the same place is a double click)
drag "$AX" "$AY" "$BOXX" "$BOXY" 'dump > "$T/dump-over-box.txt"; shot over-box'
grep -q " drop=$(idx Box)\$" "$T/dump-over-box.txt" && pass "a folder icon an icon is dragged over is highlighted" \
    || fail "no highlight over Box: $(head -1 "$T/dump-over-box.txt")"
wait_for 10 test -f "$DESK/Box/a.txt" && [ ! -e "$DESK/a.txt" ] && pass "an icon dropped on a folder icon moves into the folder" \
    || fail "a.txt: $(ls "$DESK" "$DESK/Box" | tr '\n' ' ')"
wait_for 10 eval '! has_item a.txt' && pass "... and leaves the desktop" || fail "a.txt still shown"

# --- the desktop and File Explorer --------------------------------------------------------------------
"$WINE" explorer "C:\\users\\$U\\Documents" >/dev/null 2>&1 &
wait_for 30 title_is Documents; sleep 2
dprobe place Documents 300 330 700 330 >/dev/null; sleep 1
set -- $(dprobe blank Documents | sed 's/blank=//; s/,/ /')
FBX=$1; FBY=$2
set -- $(at d.txt)
drag "$1" "$2" "$FBX" "$FBY"
wait_for 10 test -f "$DOCS/d.txt" && [ ! -e "$DESK/d.txt" ] && pass "an icon dropped on File Explorer moves into its folder" \
    || fail "d.txt: desktop $(ls "$DESK" | tr '\n' ' ') documents $(ls "$DOCS" | tr '\n' ' ')"
lv_has_d() { lv Documents | grep -q ' d\.txt$'; }
wait_for 10 lv_has_d && pass "... and File Explorer shows it" || fail "File Explorer's view: $(lv Documents | tr '\n' ' ')"
set -- $(lv_item Documents f3.txt)
if [ $# -eq 2 ]; then
    drag "$1" "$2" 200 150
    wait_for 10 test -f "$DESK/f3.txt" && [ ! -e "$DOCS/f3.txt" ] && pass "a file dragged from File Explorer onto the desktop moves into the Desktop folder" \
        || fail "f3.txt: desktop $(ls "$DESK" | tr '\n' ' ') documents $(ls "$DOCS" | tr '\n' ' ')"
    wait_for 10 has_item f3.txt; set -- $(at f3.txt)
    [ $# -eq 2 ] && near "$1" "$2" 200 150 86 && pass "... placed where it was dropped ($1,$2)" || fail "f3.txt is at $*"
    lv_no_f3() { ! lv Documents | grep -q ' f3\.txt$'; }
    wait_for 10 lv_no_f3 && pass "... and File Explorer no longer shows it" || fail "File Explorer's view: $(lv Documents | tr '\n' ' ')"
else fail "no f3.txt in File Explorer: $(lv Documents | tr '\n' ' ')"; fi
"$WINE" explorer-probe.exe close-all >/dev/null 2>&1; sleep 1

# --- Delete, Sort by --------------------------------------------------------------------------------------
set -- $(at b.txt); BX=$1; BY=$2
set -- $(at c.txt)
xdotool mousemove "$BX" "$BY" click 1; sleep 0.3
xdotool mousemove "$1" "$2" keydown ctrl click 1 keyup ctrl; sleep 0.5; xdotool key Delete
wait_for 15 eval '[ ! -e "$DESK/b.txt" ] && [ ! -e "$DESK/c.txt" ]' && pass "Delete deletes every selected icon" \
    || fail "after Delete: $(ls "$DESK" | tr '\n' ' ')"
xdotool mousemove 900 150 click 3
if wait_for 10 menu_open; then
    xdotool key o n; sleep 2
    ! "$WINE" reg query 'HKCU\Software\Stained Glass\Desktop\IconPositions' >/dev/null 2>&1 && \
        [ -z "$(dump | grep '^item' | cut -d' ' -f3 | cut -d, -f1 | grep -vx 43)" ] \
        && pass "Sort by > Name lines the icons up again" || fail "after Sort by: $(dump | tr '\n' '|')"
else fail "no desktop menu for Sort by"; fi
cp "$DUMP" "$T/dump-desktop-end.txt"

# --- File Explorer: rubber bands -------------------------------------------------------------------------
"$WINE" explorer "C:\\users\\$U\\Documents" >/dev/null 2>&1 &
wait_for 30 title_is Documents; sleep 2
dprobe place Documents 300 0 700 330 >/dev/null; dprobe view Documents details >/dev/null; sleep 1
lv Documents > "$T/lv-details.txt"
set -- $(dprobe blank Documents | sed 's/blank=//; s/,/ /'); FBX=$1; FBY=$2
set -- $(lv_item Documents f1.txt)
if [ $# -eq 2 ]; then
    shot lv-before
    drag "$FBX" "$FBY" "$1" "$2" 'shot lv-band'
    # the rows are Sub, d.txt, f1.txt, f2.txt: from below up to f1.txt, it and f2.txt
    [ "$(lv_count Documents selcount)" = 2 ] && [ "$(lv_state Documents f1.txt)" = 1 ] && [ "$(lv_state Documents f2.txt)" = 1 ] \
        && pass "Details: a rubber band from the empty view selects the rows it crosses" || fail "Details band: $(lv Documents | tr '\n' ' ')"
    b=$(pixel lv-before $((FBX - 20)) $((FBY - 20))); d=$(pixel lv-band $((FBX - 20)) $((FBY - 20)))
    [ "$b" != "$d" ] && pass "... drawn filled and translucent, not a dotted frame ($b -> $d)" || fail "the band is not filled ($b)"
else fail "no f1.txt row: $(cat "$T/lv-details.txt" | tr '\n' ' ')"; fi
dprobe view Documents icons >/dev/null; sleep 1
xdotool mousemove "$FBX" "$FBY" click 1; sleep 1   # (and no double click with the band's press)
set -- $(lv_item Documents f1.txt)
if [ $# -eq 2 ]; then
    drag "$FBX" "$FBY" "$1" "$(( $2 - 20 ))"
    [ "$(lv_count Documents selcount)" -ge 1 ] && [ "$(lv_state Documents f1.txt)" = 1 ] && [ "$(lv_state Documents Sub)" = 0 ] \
        && pass "Large icons: a rubber band selects the icons it touches" || fail "icons band: $(lv Documents | tr '\n' ' ')"
else fail "no f1.txt icon"; fi

# --- File Explorer: dragging -----------------------------------------------------------------------------
xdotool mousemove "$FBX" "$FBY" click 1; sleep 1      # f1.txt alone
set -- $(lv_item Documents f1.txt) $(lv_item Documents Sub)
[ $# -eq 4 ] || set -- 0 0 0 0
drag "$1" "$(( $2 - 20 ))" "$3" "$(( $4 - 20 ))" 'lv Documents > "$T/lv-over-sub.txt"'
grep -q ' 2 Sub$' "$T/lv-over-sub.txt" && pass "a folder a file is dragged over is highlighted" || fail "no highlight: $(tr '\n' ' ' < "$T/lv-over-sub.txt")"
wait_for 10 test -f "$DOCS/Sub/f1.txt" && [ ! -e "$DOCS/f1.txt" ] && pass "a file dropped on a folder moves into it" \
    || fail "f1.txt: $(ls -R "$DOCS" | tr '\n' ' ')"
set -- $(lv_item Documents f2.txt)
[ $# -eq 2 ] || set -- 0 0
before=$(ls "$DOCS" | tr '\n' ' ')
drag "$1" "$(( $2 - 20 ))" "$FBX" "$FBY"
sleep 1
[ "$(ls "$DOCS" | tr '\n' ' ')" = "$before" ] && pass "a file dragged a little way in its own folder is not copied" \
    || fail "files now: $(ls "$DOCS" | tr '\n' ' ') (were $before)"
echo f4 > "$DOCS/f4.txt"; xdotool mousemove "$FBX" "$FBY" click 1 key F5; sleep 2
"$WINE" explorer "C:\\users\\$U\\Music" >/dev/null 2>&1 &
wait_for 30 title_is Music; sleep 2
dprobe place Music 300 340 700 310 >/dev/null; sleep 1
set -- $(dprobe blank Music | sed 's/blank=//; s/,/ /'); MBX=$1; MBY=$2
set -- $(lv_item Documents f2.txt) $(lv_item Documents f4.txt)
if [ $# -eq 4 ]; then
    xdotool mousemove "$1" "$(( $2 - 20 ))" click 1; sleep 0.3
    xdotool mousemove "$3" "$(( $4 - 20 ))" keydown ctrl click 1 keyup ctrl; sleep 0.5
    drag "$1" "$(( $2 - 20 ))" "$MBX" "$MBY"
    wait_for 15 eval 'test -f "$MUSIC/f2.txt" && test -f "$MUSIC/f4.txt"' && [ ! -e "$DOCS/f2.txt" ] && [ ! -e "$DOCS/f4.txt" ] \
        && pass "two files dragged to another File Explorer window move there" || fail "Music: $(ls "$MUSIC" | tr '\n' ' ') Documents: $(ls "$DOCS" | tr '\n' ' ')"
    music_shows() { [ "$(lv_count Music count)" = 2 ]; }
    wait_for 10 music_shows && pass "... the window they went to shows them" || fail "Music's view: $(lv Music | tr '\n' ' ')"
    docs_lost() { [ "$(lv_count Documents count)" = 2 ]; }   # Sub, d.txt
    wait_for 10 docs_lost && pass "... the window they left no longer shows them" || fail "Documents' view: $(lv Documents | tr '\n' ' ')"
else fail "no f2.txt, f4.txt icons: $(lv Documents | tr '\n' ' ')"; fi
shot end
