#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Dark mode (patches/sg/0160-0162), live, in a shell session, by pixels.
#
# Two programs' windows are open -- "Plain", and "Dark", which asked for a dark
# title bar with DWMWA_USE_IMMERSIVE_DARK_MODE. Then, as Settings does, the
# app mode goes dark (AppsUseLightTheme=0 + WM_SETTINGCHANGE
# "ImmersiveColorSet"): the shell switches the visual style to its Dark
# colour scheme and every running program follows -- the title bar, the window
# background, the menu bar, the themed push button, in a process that did not
# make the change. A program started afterwards is dark too. The Windows mode
# (SystemUsesLightTheme) turns the taskbar light and back. Back to light, the
# windows are light again, except the title bar that asked to be dark.
#
#   WINE=/opt/wine-sg/bin/wine test/darkmode-gate.sh
#   ARTIFACTS=DIR keeps the screenshots and the probes' reports
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dark.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$WINEPREFIX"/drive_c/dark-*.txt "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/p.rc"
"$WINDRES" "$T/p.rc" -O coff -o "$T/p.o" &&
"$MINGW" -municode -O2 -o "$T/darkmode-probe.exe" "$HERE/darkmode-probe.c" "$T/p.o" \
    -lcomctl32 -luxtheme -ldwmapi -lgdi32 -luser32 -ladvapi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/darkmode-probe.exe" "$WINEPREFIX/drive_c/"
# programs join the shell's desktop, as in a session (sg-session's sg-run-explorer)
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# a new profile's modes (sg-shell's defaults): light apps, dark Windows
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v SystemUsesLightTheme /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "\$*" >> "$T/log.out"; "$WINE" darkmode-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
shot() { sleep 3; import -window root "$T/\$1.png"; for n in Plain Dark Later; do [ -f dark-\$n.txt ] && echo "\$1 \$n \$(cat dark-\$n.txt | tr -d '\r')" >> "$T/reports.out"; done; }
WINEDEBUG="err+all" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" darkmode-probe.exe window Plain 40 60 >/dev/null 2>&1 &
sleep 2
"$WINE" darkmode-probe.exe window Dark 520 60 dark >/dev/null 2>&1 &
# wait for the window rather than a fixed sleep: on a loaded host it can
# take longer than 2s to appear (a missing attribute still fails: hr=0)
i=0; while ! "$WINE" darkmode-probe.exe state 2>/dev/null | grep -q 'hr=0\b' && [ \$i -lt 30 ]; do sleep 1; i=\$((i + 1)); done
sleep 1
P state
shot light
P mode apps dark
shot dark
P state
"$WINE" darkmode-probe.exe window Later 40 360 >/dev/null 2>&1 &
shot later
P mode system light
shot syslight
P mode apps light
shot back
P state
P mode system dark
shot sysdark
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
"$WINE" reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\ThemeManager' /v ColorName 2>/dev/null | tr -d '\r' > "$T/reg.out"
"$WINE" reg query 'HKCU\Control Panel\Colors' /v Window 2>/dev/null | tr -d '\r' >> "$T/reg.out"
sed 's/^/      /' "$T/log.out" "$T/reports.out"

after() { awk -v c="$1" -v n="${2:-1}" '$0 == c { k++; if (k == n) { getline; print; exit } }' "$T/log.out"; }
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
# a pixel is dark (every channel below 80) / light (every channel above 200)
dark() { px "$@" | awk -F, '{ exit !($1 < 80 && $2 < 80 && $3 < 80) }'; }
light() { px "$@" | awk -F, '{ exit !($1 > 200 && $2 > 200 && $3 > 200) }'; }
report() { awk -v s="$1" -v n="$2" '$1 == s && $2 == n { $1 = $2 = ""; print; exit }' "$T/reports.out"; }

# window "Plain" is at 40,60 and "Dark" at 520,60, each 360x260: the title
# bar is 12 px below the top, the menu bar 38 px, the bare background the
# left half of the client area, the push button at client 200..320 x 20..54
CAP=72; MENU=98; BG=200; BTNX=300; BTNY=120
light light 150 $CAP && pass "light: Plain's title bar is light" || fail "light: Plain's title bar $(px light 150 $CAP)"
light light 150 $BG && pass "light: its background is light" || fail "light: background $(px light 150 $BG)"
dark light 630 $CAP && pass "DWMWA_USE_IMMERSIVE_DARK_MODE: Dark's title bar is dark while everything is light" || fail "the dark title bar: $(px light 630 $CAP)"
light light 630 $BG && pass "and only its title bar: its background stays light" || fail "Dark's background: $(px light 630 $BG)"
case "$(after state 1)" in *"darkattr=1 hr=0"*) pass "DwmGetWindowAttribute reports it" ;; *) fail "DwmGetWindowAttribute: $(after state 1)" ;; esac
dark light 900 690 && pass "the taskbar is dark (Windows mode dark)" || fail "taskbar: $(px light 900 690)"

dark dark 150 $CAP && pass "dark: Plain's title bar follows the app mode" || fail "dark: Plain's title bar $(px dark 150 $CAP)"
dark dark 150 $BG && pass "dark: its window background (COLOR_WINDOW, another process's colours)" || fail "dark: background $(px dark 150 $BG)"
dark dark 150 $MENU && pass "dark: its menu bar" || fail "dark: menu bar $(px dark 150 $MENU)"
dark dark $BTNX $BTNY && pass "dark: its themed push button (the Dark scheme's images)" || fail "dark: push button $(px dark $BTNX $BTNY)"
case "$(report dark Plain)" in *"window=202020 menubar=202020 scheme=Dark buttontext=ffffff"*) pass "the program itself sees the dark colours and the Dark scheme" ;; *) fail "Plain's report: $(report dark Plain)" ;; esac
case "$(after state 2)" in "window=202020 scheme=Dark"*) pass "a program started afterwards is dark" ;; *) fail "new process: $(after state 2)" ;; esac
dark later 150 400 && pass "and its window is drawn dark" || fail "later window: $(px later 150 400)"

light syslight 900 690 && pass "Windows mode light: the taskbar turns light" || fail "taskbar light: $(px syslight 900 690)"
dark syslight 150 $BG && pass "while the apps stay dark" || fail "apps after Windows mode: $(px syslight 150 $BG)"

light back 150 $CAP && light back 150 $BG && light back 150 $MENU && pass "back to light: title bar, background and menu bar" || fail "back: $(px back 150 $CAP) $(px back 150 $BG) $(px back 150 $MENU)"
light back $BTNX $BTNY && pass "back to light: the push button" || fail "back: button $(px back $BTNX $BTNY)"
dark back 630 $CAP && pass "and the title bar that asked to be dark stays dark" || fail "Dark's title bar after: $(px back 630 $CAP)"
case "$(after state 3)" in "window=ffffff scheme=Blue"*) pass "a new process sees the light scheme again" ;; *) fail "new process after: $(after state 3)" ;; esac
dark sysdark 900 690 && pass "Windows mode dark: the taskbar is dark again" || fail "taskbar dark: $(px sysdark 900 690)"
grep -q 'ColorName.*Blue' "$T/reg.out" && grep -q 'Window.*255 255 255' "$T/reg.out" \
    && pass "the light colours are saved (ThemeManager, Control Panel\\Colors)" || fail "registry: $(cat "$T/reg.out")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
