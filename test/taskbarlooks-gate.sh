#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's looks (patches/sg/0600). Settings > Personalization >
# Taskbar > Taskbar style writes HKCU\Software\Stained Glass\Taskbar Style
# and sends WM_SETTINGCHANGE "TraySettings": 0 the flat bar (the default,
# unchanged), 1 Horizon -- a 38 px bright blue gradient bar, a green Start
# button with a round end, raised blue buttons, the notification icons and
# clock on a lighter panel -- 2 Glass -- a 40 px dark glass bar with a light
# rim, a round Start orb, 60 px icon buttons in glass frames. All drawn with
# our own GDI gradients and shapes. The checks read the bar back (its
# rectangle, the work area, its buttons) and the screen, by pixels.
#
#   WINE=/opt/wine-sg/bin/wine test/taskbarlooks-gate.sh
#   ARTIFACTS=DIR keeps the screenshots and the probe's log
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v convert >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-tblooks.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
# a shortcut maker: Notepad pinned, for Horizon's Quick Launch
cat > "$T/mklnk.c" <<'EOC'
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
int wmain(int argc, WCHAR **argv)
{
    IShellLinkW *l; IPersistFile *f;
    if (argc < 3) return 2;
    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&l))) return 1;
    IShellLinkW_SetPath(l, argv[2]);
    IShellLinkW_QueryInterface(l, &IID_IPersistFile, (void **)&f);
    return FAILED(IPersistFile_Save(f, argv[1], TRUE));
}
EOC
"$MINGW" -O2 -municode -o "$T/mklnk.exe" "$T/mklnk.c" -lole32 -luuid || { fail "mklnk did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$T/mklnk.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/users/$(id -un)/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"
"$WINE" 'C:\mklnk.exe' 'C:\users\'"$(id -un)"'\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\Notepad.lnk' 'C:\windows\system32\notepad.exe' >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

ADV='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
TB='HKCU\Software\Stained Glass\Taskbar'
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
R() { "$WINE" reg add "\$1" /v "\$2" /t REG_DWORD /d "\$3" /f >/dev/null 2>&1; }
S() { echo "== \$1" >> "$T/log.out"; "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
apply() { "$WINE" taskbar-probe.exe apply >/dev/null 2>&1; sleep 3; }
shot() { import -window root "$T/\$1.png"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" taskbar-probe.exe window One 200 100 >/dev/null 2>&1 &
sleep 1
"$WINE" taskbar-probe.exe window Two 260 160 >/dev/null 2>&1 &
sleep 1
"$WINE" notepad >/dev/null 2>&1 &
i=0; while [ \$i -lt 60 ] && ! "$WINE" taskbar-probe.exe state 2>/dev/null | grep -q "^windows=3"; do sleep 1; i=\$((i + 1)); done
sleep 2
S flat; shot flat
R "$TB" Style 1; apply; S horizon; shot horizon
R "$TB" Style 2; apply; S glass; shot glass
R "$ADV" TaskbarGlomLevel 0; apply; S glassicons; shot glassicons
R "$ADV" TaskbarGlomLevel 2; R "$TB" Style 1; R "$TB" Position 1; apply; S horizontop; shot horizontop
R "$TB" Position 3; R "$TB" Style 0; apply; S flatagain; shot flatagain
# the taskbar's own look (Taskbar Look, 0802), not the windows' style
R 'HKCU\\Software\\Stained Glass\\Style' Rounded 1; R "$TB" Look 0; apply; S lookclassic
R 'HKCU\\Software\\Stained Glass\\Style' Rounded 0; R "$TB" Look 1; apply; S lookrounded
R "$TB" Look 2; apply; S lookhorizon
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

v() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { sub(k "=", ""); print; exit }' "$T/log.out"; }
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
# is the pixel's colour of a kind: blue (b well over r), green (g over r and b), dark, light
blue()  { echo "$1" | awk -F, '{ exit !($3 > $1 + 80 && $3 > 150) }'; }
green() { echo "$1" | awk -F, '{ exit !($2 > $1 + 40 && $2 > $3 + 40) }'; }
dark()  { echo "$1" | awk -F, '{ exit !($1 < 96 && $2 < 110 && $3 < 130 && $3 > $1) }'; }
light() { echo "$1" | awk -F, '{ exit !($1 + $2 + $3 > 330) }'; }
field() { echo "$1" | cut -d, -f"$2"; }
[ -f "$T/flatagain.png" ] || { fail "the session did not finish"; echo "RESULT: FAIL"; exit 1; }

# the default stays exactly as it was
[ "$(v flat bar)" = "0,660,1024,700 topmost=1" ] && [ "$(px flat 600 690)" = "31,31,31" ] \
    && pass "the default look: the flat 40 px #1F1F1F bar" || fail "default: $(v flat bar) $(px flat 600 690)"

# Horizon
[ "$(v horizon bar)" = "0,662,1024,700 topmost=1" ] && [ "$(v horizon work)" = "0,0,1024,662" ] \
    && pass "Horizon: a 38 px bar, its space reserved" || fail "Horizon bar: $(v horizon bar) / $(v horizon work)"
b=$(px horizon 700 681); top=$(px horizon 700 662); foot=$(px horizon 700 699)
blue "$b" && [ "$top" != "$b" ] && [ "$foot" != "$b" ] && [ "$(field "$top" 3)" -gt "$(field "$foot" 3)" ] \
    && pass "a blue gradient bar, lit at the top ($top / $b / $foot)" || fail "Horizon bar colours: top $top mid $b foot $foot"
s=$(v horizon start); sw=$(( $(field "$s" 3) - $(field "$s" 1) )); sx=$(( $(field "$s" 3) - 12 ))
green "$(px horizon "$sx" 684)" && green "$(px horizon 40 697)" && [ "$sw" -ge 96 ] \
    && pass "a green Start button, $sw px wide ($(px horizon "$sx" 684))" || fail "Horizon Start: $s $(px horizon "$sx" 684) $(px horizon 40 697)"
se=$(( $(field "$s" 3) - 2 ))
[ "$(px horizon "$se" 663)" != "$(px horizon "$sx" 663)" ] \
    && pass "with a round end (its corner is the bar: $(px horizon "$se" 663))" || fail "Start end not round: $(px horizon "$se" 663)"
p=$(px horizon 1010 667)
blue "$p" && [ "$p" != "$b" ] && [ "$(field "$p" 2)" -gt "$(field "$b" 2)" ] \
    && pass "the notification area and clock on a lighter panel ($p)" || fail "Horizon panel: $p (bar $b)"
bt=$(v horizon button); bx=$(( $(field "$bt" 1) + 4 ))
bb=$(px horizon "$bx" 681); gap=$(px horizon $(( $(field "$bt" 1) )) 681)
[ "$bb" != "$b" ] && blue "$bb" && pass "a window's button is a raised blue button ($bb)" || fail "Horizon button: $bb (bar $b)"
[ "$(v horizontop bar)" = "0,0,1024,38 topmost=1" ] && blue "$(px horizontop 700 19)" \
    && pass "Horizon at the top edge" || fail "Horizon top: $(v horizontop bar) $(px horizontop 700 19)"
# Quick Launch: Notepad pinned and running -- in Horizon its pin stays, a
# small icon of its own before the window buttons; the flat look puts the
# running window in the pin's place instead
pn=$(v horizon pin); pw=0; [ -n "$pn" ] && pw=$(( $(field "$pn" 3) - $(field "$pn" 1) ))
[ -n "$pn" ] && [ "$pw" = 26 ] && [ "$(field "$pn" 3)" -le "$(field "$bt" 1)" ] \
    && pass "Quick Launch: the pinned program keeps a 26 px icon while it runs, before the window buttons ($pn)" \
    || fail "Quick Launch: pin '$pn' (width $pw) button $bt"
[ -z "$(v flat pin)" ] && pass "the flat look: the running window takes its pin's place" || fail "flat pin: $(v flat pin)"

# Glass
[ "$(v glass bar)" = "0,660,1024,700 topmost=1" ] && pass "Glass: a 40 px bar" || fail "Glass bar: $(v glass bar)"
g=$(px glass 700 690); rim=$(px glass 700 660)
dark "$g" && light "$rim" && pass "dark glass ($g) with a light rim ($rim)" || fail "Glass colours: $g rim $rim"
s=$(v glass start); cx=$(( ($(field "$s" 1) + $(field "$s" 3)) / 2 )); corner=$(( $(field "$s" 1) + 1 ))
o=$(px glass "$cx" 693)
[ "$(px glass "$corner" 662)" = "$(px glass 700 662)" ] && [ "$o" != "$(px glass 700 693)" ] && echo "$o" | awk -F, '{ exit !($3 > $1 + 40) }' \
    && pass "a round Start orb: its button's corner is glass, its body blue-green ($o)" || fail "Glass orb: corner $(px glass "$corner" 662) body $o"
bt=$(v glassicons button); w=$(( $(field "$bt" 3) - $(field "$bt" 1) ))
[ "$w" = 60 ] && pass "icon buttons are 60 px wide" || fail "Glass icon button: $bt"
fx=$(( $(field "$bt" 1) + 2 )); fr=$(px glassicons "$fx" 680)
# Notepad's button, the second: its program's 32 px icon (many colours) in it
nb=$(awk '$0 == "== glassicons" { on = 1; next } /^== / { on = 0 } on && /^button=/ { n++; if (n == 2) { sub(/^button=/, ""); print } }' "$T/log.out")
ncx=$(( ($(field "$nb" 1) + $(field "$nb" 3)) / 2 ))
colours=$(convert "$T/glassicons.png" -crop 32x32+$((ncx - 16))+664 +repage -format %k info: 2>/dev/null)
[ "$(( $(field "$nb" 3) - $(field "$nb" 1) ))" = 60 ] && [ "${colours:-0}" -gt 30 ] \
    && pass "and a big icon in it ($colours colours in its middle 32 px)" || fail "Glass big icon: $nb, $colours colours"
[ "$fr" != "$(px glassicons 700 680)" ] && pass "a running program's button has a glass frame ($fr)" || fail "Glass frame: $fr"

# and back
[ "$(v flatagain bar)" = "0,660,1024,700 topmost=1" ] && [ "$(px flatagain 600 690)" = "31,31,31" ] \
    && pass "Style 0: the flat bar again" || fail "back to flat: $(v flatagain bar) $(px flatagain 600 690)"

# mixed: each part its own look (David 2026-10-02) -- the taskbar goes by
# Taskbar Look, whatever the windows' style
[ "$(v lookclassic bar)" = "0,660,1024,700 topmost=1" ] && [ "$(v lookrounded bar)" = "0,652,1024,700 topmost=1" ] \
    && [ "$(v lookhorizon bar)" = "0,662,1024,700 topmost=1" ] \
    && pass "Taskbar Look: a Classic bar with round windows, a Rounded bar with square ones, Horizon by its Look alone" \
    || fail "Taskbar Look: classic $(v lookclassic bar) rounded $(v lookrounded bar) horizon $(v lookhorizon bar)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
