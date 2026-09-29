#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar honours Settings > Personalization > Taskbar (patches/sg/0164).
#
# Settings writes the values where Windows keeps them and sends
# WM_SETTINGCHANGE "TraySettings"; the probe does exactly that, then reads the
# bar back: its rectangle, the work area it reserves, SHAppBarMessage's
# answers and its visible buttons -- and the screen, by pixels. Small
# buttons, combining one program's buttons, centring, the search box and
# icon, hiding Task View, the four edges, auto-hide (it tucks away, the
# pointer at the edge brings it back, nothing is reserved) and a program
# turning auto-hide off with ABM_SETSTATE.
#
#   WINE=/opt/wine-sg/bin/wine test/taskbar-gate.sh
#   ARTIFACTS=DIR keeps the screenshots and the probe's log
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-taskbar.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

ADV='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
TB='HKCU\Software\Stained Glass\Taskbar'
SEARCH='HKCU\Software\Microsoft\Windows\CurrentVersion\Search'
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
R() { "$WINE" reg add "\$1" /v "\$2" /t REG_DWORD /d "\$3" /f >/dev/null 2>&1; }
S() { echo "== \$1" >> "$T/log.out"; "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
apply() { "$WINE" taskbar-probe.exe apply >/dev/null 2>&1; sleep 2; }
shot() { import -window root "$T/\$1.png"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" taskbar-probe.exe window One 200 100 >/dev/null 2>&1 &
sleep 1
"$WINE" taskbar-probe.exe window Two 260 160 >/dev/null 2>&1 &
sleep 1
"$WINE" notepad >/dev/null 2>&1 &
# the three windows' buttons (a loaded machine starts programs slowly)
i=0; while [ \$i -lt 60 ] && ! "$WINE" taskbar-probe.exe state 2>/dev/null | grep -q "^windows=3"; do sleep 1; i=\$((i + 1)); done
sleep 1
S default; shot default
R "$ADV" TaskbarSmallIcons 1; apply; S small; shot small
R "$ADV" TaskbarGlomLevel 0; apply; S combine
R "$ADV" TaskbarAl 1; apply; S center; shot center
R "$SEARCH" SearchboxTaskbarMode 2; apply; S searchbox; shot searchbox
R "$SEARCH" SearchboxTaskbarMode 1; apply; S searchicon
R "$ADV" ShowTaskViewButton 0; apply; S notaskview
R "$TB" Position 1; apply; S top; shot top
R "$TB" Position 0; apply; S left; shot left
R "$TB" Position 2; apply; S right
R "$TB" Position 3; R "$ADV" TaskbarSmallIcons 0; R "$ADV" TaskbarAl 0; R "$TB" AutoHide 1; apply
xdotool mousemove 500 300; sleep 3; S hidden; shot hidden
xdotool mousemove 500 699; sleep 2; S shown; shot shown
xdotool mousemove 500 300; sleep 3; S hiddenagain
"$WINE" taskbar-probe.exe autohide 0 >/dev/null 2>&1; sleep 2; S setstate
"$WINE" reg query "$TB" /v AutoHide 2>/dev/null | tr -d '\r' > "$T/reg.out"
R "$TB" AutoHide 0; apply
R "$TB" ShowDesktops 0; apply; S nopager
R "$TB" ShowDesktops 1; apply; S pager
R "$TB" Color 1; apply; shot cdark
R "$TB" Color 2; apply; shot clight
R "$TB" Color 3; apply; shot cblue
R "$TB" Color 4; apply; shot caccent
R "$TB" Color 0; apply
R 'HKCU\Software\Stained Glass\Style' Rounded 1; "$WINE" taskbar-probe.exe mode >/dev/null 2>&1; sleep 2; S rounded; shot rounded
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

# a value in the state taken at a step
v() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { sub(k "=", ""); print; exit }' "$T/log.out"; }
count() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { n++ } END { print n + 0 }' "$T/log.out"; }
width() { echo "$1" | awk -F, '{ print $3 - $1 }'; }
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }

[ "$(v default bar)" = "0,660,1024,700 topmost=0" ] && [ "$(v default work)" = "0,0,1024,660" ] \
    && pass "default: a 40 px bar at the bottom, its space reserved" || fail "default: $(v default bar) / $(v default work)"
case "$(v default appbar)" in "2 edge=3 rc=0,660,1024,700") pass "SHAppBarMessage: bottom edge, always on top, not auto-hide" ;; *) fail "appbar: $(v default appbar)" ;; esac
[ "$(v default windows)" = 3 ] && [ "$(count default search)" = 0 ] && [ "$(count default taskview)" = 1 ] \
    && pass "a labelled button per window, Task View shown, no search" || fail "default buttons: windows $(v default windows) search $(count default search) taskview $(count default taskview)"

[ "$(v small bar)" = "0,670,1024,700 topmost=0" ] && [ "$(v small work)" = "0,0,1024,670" ] \
    && pass "small taskbar buttons: a 30 px bar" || fail "small: $(v small bar) / $(v small work)"
[ "$(v combine windows)" = 2 ] && [ "$(width "$(v combine button)")" = 48 ] \
    && pass "combine buttons: one program's two windows share one icon button" || fail "combine: windows $(v combine windows) first $(v combine button)"
s=$(v center start); [ "${s%%,*}" -gt 100 ] 2>/dev/null && pass "centred: Start moves in from the left ($s)" || fail "centre: start $s"
# the group (Start to the last window button) is centred on the whole bar,
# not in the room left of the notification area
gr=$(awk '$0 == "== center" { on = 1; next } /^== / { on = 0 } on && /^(start|button)=/ { sub(/^[a-z]+=/, ""); split($0, r, ","); if (l == "" || r[1] < l) l = r[1]; if (r[3] > rr) rr = r[3] } END { print l "," rr }' "$T/log.out")
mid=$(( (${gr%%,*} + ${gr##*,}) / 2 ))
[ "$mid" -ge 508 ] && [ "$mid" -le 516 ] && pass "centred on the screen: Start and the buttons span $gr (middle $mid of 1024)" || fail "centred group: $gr (middle $mid, want 512)"
[ "$(width "$(v searchbox search)")" = 280 ] && pass "search box shown" || fail "search box: $(v searchbox search)"
[ "$(width "$(v searchicon search)")" = 48 ] && pass "search icon shown" || fail "search icon: $(v searchicon search)"
[ "$(count notaskview taskview)" = 0 ] && pass "Task View button hidden" || fail "Task View still shown"

[ "$(v top bar)" = "0,0,1024,30 topmost=0" ] && [ "$(v top work)" = "0,30,1024,700" ] \
    && pass "position top: the bar at the top, the work area below it" || fail "top: $(v top bar) / $(v top work)"
case "$(v top appbar)" in "2 edge=1 "*) pass "SHAppBarMessage says the top edge" ;; *) fail "top appbar: $(v top appbar)" ;; esac
[ "$(px top 1000 15)" != "$(px small 1000 15)" ] && pass "and it is drawn there" || fail "nothing drawn at the top: $(px top 1000 15)"
[ "$(v left bar)" = "0,0,48,700 topmost=0" ] && [ "$(v left work)" = "48,0,1024,700" ] \
    && pass "position left: a vertical bar" || fail "left: $(v left bar) / $(v left work)"
b=$(v left button); [ "$(echo "$b" | awk -F, '{ print $3 - $1 }')" = 48 ] && [ "$(echo "$b" | awk -F, '{ print $4 - $2 }')" = 48 ] \
    && pass "with square buttons down it" || fail "left buttons: $b"
[ "$(v right bar)" = "976,0,1024,700 topmost=0" ] && [ "$(v right work)" = "0,0,976,700" ] \
    && pass "position right" || fail "right: $(v right bar) / $(v right work)"

[ "$(v hidden bar)" = "0,698,1024,738 topmost=1" ] && pass "auto-hide: the bar tucks into the edge when the pointer is away" || fail "auto-hide: $(v hidden bar)"
[ "$(v hidden work)" = "0,0,1024,700" ] && pass "and reserves nothing" || fail "auto-hide work area: $(v hidden work)"
case "$(v hidden appbar)" in "3 "*) [ "$(v hidden autohidebar)" = 1 ] && pass "SHAppBarMessage: ABS_AUTOHIDE, and the bottom's auto-hide bar" || fail "autohide bar: $(v hidden autohidebar)" ;; *) fail "hidden appbar: $(v hidden appbar)" ;; esac
[ "$(px hidden 900 680)" != "$(px default 900 680)" ] && pass "the screen shows what is behind it" || fail "the bar is still drawn: $(px hidden 900 680)"
[ "$(v shown bar)" = "0,660,1024,700 topmost=1" ] && pass "the pointer at the bottom edge brings it back" || fail "shown: $(v shown bar)"
[ "$(px shown 900 680)" = "$(px default 900 680)" ] && pass "and it is drawn" || fail "not drawn when shown: $(px shown 900 680)"
[ "$(v hiddenagain bar)" = "0,698,1024,738 topmost=1" ] && pass "and it hides again" || fail "hide again: $(v hiddenagain bar)"
[ "$(v setstate bar)" = "0,660,1024,700 topmost=0" ] && [ "$(v setstate work)" = "0,0,1024,660" ] && grep -q 'AutoHide.*0x0' "$T/reg.out" \
    && pass "ABM_SETSTATE from a program turns auto-hide off (and Settings sees it)" || fail "ABM_SETSTATE: $(v setstate bar) $(v setstate work) $(cat "$T/reg.out")"
[ "$(count pager pager)" = 1 ] && [ "$(count nopager pager)" = 0 ] \
    && pass "ShowDesktops 0 hides the virtual desktops' pager, 1 shows it" || fail "pager: shown $(count pager pager), hidden $(count nopager pager)"
[ "$(px cdark 600 690)" = "31,31,31" ] && pass "taskbar color Dark: #1F1F1F" || fail "dark: $(px cdark 600 690)"
[ "$(px clight 600 690)" = "238,238,238" ] && pass "taskbar color Light" || fail "light: $(px clight 600 690)"
[ "$(px cblue 600 690)" = "233,238,246" ] && pass "taskbar color Light blue (whitish blue)" || fail "light blue: $(px cblue 600 690)"
[ "$(px caccent 600 690)" = "75,23,124" ] && pass "taskbar color Accent: the accent darkened" || fail "accent: $(px caccent 600 690)"
[ "$(v rounded bar)" = "0,652,1024,700 topmost=0" ] && [ "$(v rounded work)" = "0,0,1024,652" ] \
    && pass "the Rounded style: a 48 px bar, its space reserved" || fail "rounded bar: $(v rounded bar) / $(v rounded work)"
b=$(v rounded button); bx=$(( ($(echo "$b" | cut -d, -f1) + $(echo "$b" | cut -d, -f3)) / 2 ))
pill=$(px rounded $bx 696); edge=$(px rounded $((bx - 12)) 696)
[ "$pill" != "$edge" ] && pass "a running program's button has a short pill at its foot ($pill on $edge)" || fail "pill: $pill / beside it $edge"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
