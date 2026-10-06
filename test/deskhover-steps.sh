# The steps of deskhover-gate.sh, run in its Xvfb session (sourced: T, WINE,
# WINESERVER, DESKCOMP and DUMP are set). Each check appends PASS or FAIL to
# $T/results.txt.
R="$T/results.txt"; : > "$R"
pass() { printf 'PASS  %s\n' "$*" >> "$R"; }
fail() { printf 'FAIL  %s\n' "$*" >> "$R"; }
note() { printf 'NOTE  %s\n' "$*" >> "$R"; }
shot() { import -window root "$T/$1.png" 2>/dev/null; }
wait_for() {   # a command that succeeds, within $1 seconds
    t=$1; shift; i=0
    while ! "$@" && [ $i -lt $((t * 4)) ]; do sleep 0.25; i=$((i + 1)); done; "$@"
}
dump() { { tr -d '\r' < "$DUMP"; } 2>/dev/null; }
field() { dump | head -1 | tr ' ' '\n' | sed -n "s/^$1=//p"; }
idx() { dump | grep "^item .* $1\$" | head -1 | cut -d' ' -f2; }
has_item() { dump | grep -q "^item .* $1\$"; }
selected() { field sel | tr ',' '\n' | grep -qx "$(idx "$1")"; }
box() { dump | grep "^box $(idx "$1") " | cut -d' ' -f3 | tr ',' ' '; }      # left top right bottom
icon() { dump | grep "^box $(idx "$1") " | cut -d' ' -f5 | tr ',' ' '; }
tipline() { dump | grep '^tip ' | head -1; }
# a pixel of a screenshot, "r g b"
px() { python3 -c "
from PIL import Image
print('%d %d %d' % Image.open('$T/$1.png').convert('RGB').getpixel(($2, $3)))"; }
# |a - b| per channel <= tolerance: close "r g b" "r g b" tol
close() { python3 -c "import sys; a=[int(v) for v in sys.argv[1].split()]; b=[int(v) for v in sys.argv[2].split()]; sys.exit(0 if len(a) == 3 and len(b) == 3 and all(abs(x-y)<=int(sys.argv[3]) for x,y in zip(a,b)) else 1)" "$1" "$2" "$3"; }
brightness() { set -- $1; echo $(( $1 + $2 + $3 )); }
dist() { set -- $1 $2; echo $(( ($1 > $4 ? $1 - $4 : $4 - $1) + ($2 > $5 ? $2 - $5 : $5 - $2) + ($3 > $6 ? $3 - $6 : $6 - $3) )); }
ACCENT="0 120 215"
DPI=96

start_desktop() {   # $1: a name for this round
    cd "$(dirname "$DUMP")" || exit 1
    export SG_DESKTOP_DUMP='C:\desk.txt'
    rm -f "$DUMP"
    "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer-$1.txt" 2>&1 &
    if ! wait_for 40 has_item 'b.txt'; then
        # the shell sometimes stalls for minutes painting its taskbar on its
        # first start after a settings change (not this gate's subject): again
        note "$1: the desktop did not come up in 40 s; started again"
        "$WINESERVER" -k 2>/dev/null; sleep 2; rm -f "$DUMP"
        "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer-$1.txt" 2>&1 &
        wait_for 60 has_item 'b.txt' || fail "$1: the desktop did not come up"
    fi
    sleep 2
    # (a new display scale: the desktop lays its icons out again once it is up)
    wait_for 20 sh -c "grep -q '^metrics .*dpi=$DPI\$' '$DUMP'"
    sleep 1
    cp "$DUMP" "$T/dump-start-$1.txt"
}

stop_desktop() {
    [ -n "${CP:-}" ] && kill "$CP" 2>/dev/null; CP=
    "$WINESERVER" -k 2>/dev/null; sleep 2
}

start_deskcomp() {
    CP=
    [ -x "$DESKCOMP" ] || { note "$1: no sg-deskcomp at $DESKCOMP -- the compositor's rounds are not run"; return 1; }
    "$DESKCOMP" -dump "$T/deskcomp-$1.txt" > "$T/deskcomp-$1.log" 2>&1 & CP=$!
    if wait_for 40 grep -q 'background=1' "$T/deskcomp-$1.txt"; then sleep 1; return 0; fi
    fail "$1: sg-deskcomp did not composite the desktop: $(cat "$T/deskcomp-$1.log")"; return 1
}

# the band: pressed on the empty desktop right of the icons, dragged left and
# down over a.txt and b.txt, the screen taken while the button is still down
band_round() {   # $1 round name
    n=$1
    xdotool mousemove 900 650; sleep 0.8
    set -- $(box a.txt); ax0=$1; ay0=$2; ax1=$3; ay1=$4
    set -- $(box b.txt); by1=$4
    x0=$(( ax1 + 120 * DPI / 96 )); y0=$(( ay0 + 5 ))
    x1=$(( ax0 + (ax1 - ax0) / 2 )); y1=$(( by1 - 5 ))
    [ $y1 -gt 560 ] && y1=560   # (above the taskbar)
    shot "$n-band-before"
    xdotool mousemove "$x0" "$y0" mousedown 1; sleep 0.3
    xdotool mousemove $(( x0 - 10 )) $(( y0 + 10 )); sleep 0.2
    xdotool mousemove $(( (x0 + x1) / 2 )) $(( (y0 + y1) / 2 )); sleep 0.2
    xdotool mousemove "$x1" "$y1"; sleep 0.3
    # the mouse still moving (a hand never holds still): the band follows it,
    # drawn as it goes -- not only once the mouse rests
    ( k=0; while [ $k -lt 40 ]; do xdotool mousemove $(( x1 + k % 2 * 4 )) $(( y1 - k % 2 * 4 )); sleep 0.05; k=$((k + 1)); done ) & MV=$!
    sleep 1
    shot "$n-band"
    field band > "$T/$n-bandfield.txt"
    wait $MV
    xdotool mousemove "$x1" "$y1"; sleep 0.5
    xdotool mouseup 1; sleep 1
    shot "$n-band-after"
    # inside the band, on the empty desktop: the accent over the wallpaper
    ix=$(( x0 - 6 )); iy=$(( (y0 + y1) / 2 ))
    b=$(px "$n-band-before" $ix $iy); d=$(px "$n-band" $ix $iy)
    set -- $b; br=$1; bg=$2; bb=$3
    exp="$(( br + (0 - br) * 88 / 255 )) $(( bg + (120 - bg) * 88 / 255 )) $(( bb + (215 - bb) * 88 / 255 ))"
    [ "$(cat "$T/$n-bandfield.txt")" = 1 ] && pass "$n: a press on the empty desktop and a drag makes a rubber band" \
        || fail "$n: no band while dragging: $(dump | head -1)"
    close "$d" "$exp" 14 && pass "$n: mid-drag the band is on the screen, the accent translucent over the wallpaper ($b -> $d)" \
        || fail "$n: mid-drag the band is not drawn: at $ix,$iy $b -> $d (want about $exp)"
    # its right border (where the press was) and its top border: the accent, solid
    e=$(px "$n-band" "$x0" "$iy"); f=$(px "$n-band" $(( x0 - 20 )) "$y0")
    close "$e" "$ACCENT" 3 && close "$f" "$ACCENT" 3 && pass "$n: ... with a solid one-pixel accent border ($e, $f)" \
        || fail "$n: the band's border is not the accent: right $e, top $f"
    a=$(px "$n-band-after" $ix $iy)
    [ "$a" = "$b" ] && pass "$n: ... and gone when the button is let go" || fail "$n: the band stayed: $b -> $a"
    [ "$(field selcount)" = 2 ] && selected a.txt && selected b.txt && pass "$n: on release the icons it touched are selected" \
        || fail "$n: band selection: $(dump | head -1)"
    # nothing selected again: a click on the empty desktop
    xdotool mousemove 900 650 click 1; sleep 1
}

hover_round() {   # $1 round name
    n=$1
    LONG='A rather long file name that the desktop cuts short.txt'
    xdotool mousemove 900 650; sleep 1
    shot "$n-hover-before"
    set -- $(box "$LONG"); l=$1; t=$2; r=$3; bt=$4
    set -- $(icon "$LONG"); il=$1; it=$2; ir=$3; ib=$4
    cx=$(( (il + ir) / 2 )); cy=$(( (it + ib) / 2 ))
    # inside the box, left of the picture, and the box's left border
    qx=$(( l + 3 * DPI / 96 + 1 )); qy=$(( it + 2 ))
    [ $qx -lt $il ] || qx=$(( l + 2 ))
    xdotool mousemove "$cx" "$cy"; sleep 0.25
    shot "$n-hover-lit"
    hot=$(field hot)
    b=$(px "$n-hover-before" $qx $qy); h=$(px "$n-hover-lit" $qx $qy)
    eb=$(px "$n-hover-before" "$l" $(( (t + bt) / 2 ))); eh=$(px "$n-hover-lit" "$l" $(( (t + bt) / 2 )))
    [ "$hot" = "$(idx "$LONG")" ] && pass "$n: the icon under the pointer is the hot one" || fail "$n: hot=$hot, want $(idx "$LONG")"
    [ "$(dist "$h" "$b")" -ge 20 ] && [ "$(dist "$eh" "$eb")" -gt "$(dist "$h" "$b")" ] \
        && pass "$n: ... lit at once: a light box behind it ($b -> $h), its border stronger ($eh)" \
        || fail "$n: the icon under the pointer is not lit: inside $b -> $h, border $eb -> $eh"
    # the infotip, after the hover delay
    wait_for 4 sh -c "grep -q '^tip .*visible=1' '$DUMP'"
    sleep 0.5
    shot "$n-hover-tip"
    tl=$(tipline)
    echo "$tl" > "$T/$n-tip.txt"
    case "$tl" in *"visible=1 $LONG|Type: "*"|Size: "*"|Date modified: "*) pass "$n: after the delay an infotip: the full name, Type, Size, Date modified ($tl)";;
        *) fail "$n: no infotip with the full name and details: '$tl'";; esac
    set -- $(echo "$tl" | cut -d' ' -f2 | tr ',' ' ')
    if [ $# -eq 4 ] && [ "$2" -ge $(( cy + 8 )) ] && [ "$1" -le $(( cx + 8 )) ] && [ "$3" -gt "$cx" ]; then
        tx=$(( ($1 + $3) / 2 )); ty=$(( $2 + 3 ))
        [ "$(px "$n-hover-before" $tx $ty)" != "$(px "$n-hover-tip" $tx $ty)" ] && pass "$n: ... shown below the pointer ($1,$2-$3,$4)" \
            || fail "$n: the infotip window is not on the screen at $tx,$ty"
    else fail "$n: the infotip is not below the pointer at $cx,$cy: $*"; fi
    # off the icon: no light, no tip
    xdotool mousemove 900 650; sleep 0.8
    shot "$n-hover-off"
    o=$(px "$n-hover-off" $qx $qy)
    [ "$(field hot)" = -1 ] && [ "$(field tip)" = -1 ] && [ "$o" = "$b" ] && pass "$n: off the icon, the light and the tip go" \
        || fail "$n: after leaving: hot=$(field hot) tip=$(field tip) pixel $b -> $o"
    # a selected icon under the pointer: stronger than lit alone
    xdotool mousemove "$cx" "$cy"; sleep 0.6; xdotool click 1; sleep 0.8
    shot "$n-hover-sel"
    s=$(px "$n-hover-sel" $qx $qy)
    [ "$(dist "$s" "$b")" -gt "$(dist "$h" "$b")" ] && pass "$n: selected and under the pointer: stronger ($h -> $s)" \
        || fail "$n: a selected icon under the pointer is not stronger: $h -> $s"
    xdotool mousemove 900 650 click 1; sleep 1
}

keys_round() {
    n=$1
    xdotool mousemove 900 650 click 1; sleep 0.8
    xdotool key Home; sleep 0.8
    first=$(field selected)
    xdotool key Down; sleep 0.8
    second=$(field selected)
    [ "$(field focuscues)" = 1 ] && [ "$(field selcount)" = 1 ] && [ "$first" != "$second" ] && [ "$second" != -1 ] \
        && pass "$n: Home and Down move the focus and the selection ($first -> $second)" \
        || fail "$n: keyboard: $(dump | head -1) (first $first)"
    shot "$n-keys-before"
    xdotool key ctrl+Down; sleep 0.8
    third=$(field focus)
    shot "$n-keys-focus"
    set -- $(dump | grep "^box $third " | cut -d' ' -f3 | tr ',' ' ')
    if [ $# -eq 4 ] && [ "$third" != "$second" ]; then
        a=$(px "$n-keys-before" "$1" $(( ($2 + $4) / 2 ))); c=$(px "$n-keys-focus" "$1" $(( ($2 + $4) / 2 )))
        [ "$(field selcount)" = 1 ] && [ "$a" != "$c" ] && pass "$n: Ctrl+Down moves the focus alone, and the focused icon is marked ($a -> $c)" \
            || fail "$n: Ctrl+Down: $(dump | head -1) border $a -> $c"
    else fail "$n: Ctrl+Down did not move the focus: $(dump | head -1)"; fi
    xdotool mousemove 900 650 click 1; sleep 0.8
}

ROUNDS=${SG_DESKHOVER_ROUNDS:-plain comp scaled}
case " $ROUNDS " in *" plain "*)
# --- 100 %, X alone ---------------------------------------------------------------------
start_desktop plain
band_round plain
hover_round plain
keys_round plain
stop_desktop
;; esac

case " $ROUNDS " in *" comp "*)
# --- 100 %, the desktop's compositor ------------------------------------------------------
start_desktop comp
if start_deskcomp comp; then
    band_round comp
    hover_round comp
fi
stop_desktop
;; esac
case " $ROUNDS " in *" scaled "*) ;; *) return 0 2>/dev/null || exit 0;; esac

# --- 200 %, a light wallpaper, the Rounded look, the compositor ---------------------------
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 192 /f >/dev/null 2>&1
# a light wallpaper: a picture of one light grey
python3 -c "from PIL import Image; Image.new('RGB', (64, 64), (235, 235, 235)).save('$(dirname "$DUMP")/light.bmp')"
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d 'C:\light.bmp' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v WallpaperStyle /d 2 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Rounded /t REG_DWORD /d 1 /f >/dev/null 2>&1
# (reg's process starts the shell's desktop: gone, and the registry saved)
"$WINESERVER" -k 2>/dev/null; sleep 2
DPI=192
start_desktop scaled
[ "$(dump | sed -n 's/^metrics .*dpi=//p')" = 192 ] && pass "scaled: the desktop is at 200 %" || fail "scaled: $(dump | grep '^metrics')"
start_deskcomp scaled
band_round scaled
hover_round scaled
# the Rounded look: the lit box's corner is round (the corner pixel is the wallpaper's)
LONG='A rather long file name that the desktop cuts short.txt'
set -- $(box "$LONG"); l=$1; t=$2; r=$3
set -- $(icon "$LONG"); cx=$(( ($1 + $3) / 2 )); cy=$(( ($2 + $4) / 2 ))
xdotool mousemove 900 650; sleep 0.8; shot scaled-round-before
xdotool mousemove "$cx" "$cy"; sleep 0.4; shot scaled-round
c0=$(px scaled-round-before "$l" "$t"); c1=$(px scaled-round "$l" "$t"); m0=$(px scaled-round-before $(( (l + r) / 2 )) "$t"); m1=$(px scaled-round $(( (l + r) / 2 )) "$t")
[ "$c0" = "$c1" ] && [ "$m0" != "$m1" ] && pass "scaled: the Rounded look's box has round corners (corner $c1, top edge $m0 -> $m1)" \
    || fail "scaled: corners: corner $c0 -> $c1, top edge $m0 -> $m1"
stop_desktop
