#!/bin/sh
# The Windows key's shortcuts (patches/sg/0071), typed on the X keyboard.
#
# Win+Left/Right snap the active window to half the work area and back,
# Win+Up maximizes, Win+Down restores then minimizes, Win+D shows the
# desktop and puts the windows back, the Windows key alone opens Start,
# Win+R the Run dialog, Win+E File Explorer.
#
#   WINE=/opt/wine-sg/bin/wine test/shellkeys-gate.sh     (ARTIFACTS=DIR keeps logs)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-shellkeys.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "\$*" >> "$T/log.out"; "$WINE" vdesk-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
K() { echo "key \$*" >> "$T/log.out"; xdotool key "\$@"; sleep 1.2; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 2
"$WINE" vdesk-probe.exe window Beta 400 200 >/dev/null 2>&1 &
sleep 3
P workarea; P rect Beta
K super+Left; P rect Beta
K super+Right; P rect Beta
K super+Right; P rect Beta
K super+Down; P rect Beta
K super+Up; P rect Beta
K super+Down; P rect Beta
K super+Down; P rect Beta
P foreground; xdotool key super+d; sleep 0.3; xdotool key super+d; sleep 1.5
P rect Alpha
K super+d; P rect Alpha; P rect Beta
K super+d; P rect Alpha
K super; P find '#32768'
K Escape
K super+r; P find '#32770' Run
K Escape
K super+e; P find ExplorerWClass
EOF
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

after() { awk -v c="$1" -v n="${2:-1}" '$0 == c { k++; if (k == n) { getline; print; exit } }' "$T/log.out"; }
work=$(after workarea | sed 's/work=//'); IFS=, read -r wl wt wr wb <<W
$work
W
mid=$(( (wl + wr) / 2 ))
orig=$(after 'rect Beta' 1)
[ "$(after 'rect Beta' 2)" = "rect=$wl,$wt,$mid,$wb zoomed=0 iconic=0" ] && pass "Win+Left snaps the window to the left half" || fail "Win+Left: $(after 'rect Beta' 2) (work $work)"
[ "$(after 'rect Beta' 3)" = "$orig" ] && pass "Win+Right from the left brings it back where it was" || fail "Win+Right back: $(after 'rect Beta' 3), was $orig"
[ "$(after 'rect Beta' 4)" = "rect=$mid,$wt,$wr,$wb zoomed=0 iconic=0" ] && pass "Win+Right snaps it to the right half" || fail "Win+Right: $(after 'rect Beta' 4)"
[ "$(after 'rect Beta' 5)" = "$orig" ] && pass "Win+Down restores a snapped window" || fail "Win+Down from snapped: $(after 'rect Beta' 5)"
case "$(after 'rect Beta' 6)" in *"zoomed=1 iconic=0") pass "Win+Up maximizes" ;; *) fail "Win+Up: $(after 'rect Beta' 6)" ;; esac
case "$(after 'rect Beta' 7)" in *"zoomed=0 iconic=0") pass "Win+Down restores a maximized window" ;; *) fail "Win+Down from maximized: $(after 'rect Beta' 7)" ;; esac
case "$(after 'rect Beta' 8)" in *"iconic=1") pass "Win+Down again minimizes it" ;; *) fail "Win+Down minimize: $(after 'rect Beta' 8)" ;; esac
case "$(after 'rect Alpha' 2)" in *"iconic=1") pass "Win+D minimizes the windows to show the desktop" ;; *) fail "Win+D: $(after 'rect Alpha' 2)" ;; esac
case "$(after 'rect Alpha' 3)" in *"iconic=0") pass "and Win+D again puts them back" ;; *) fail "Win+D back: $(after 'rect Alpha' 3)" ;; esac
[ "$(after "find #32768")" = "found=1" ] && pass "the Windows key alone opens Start" || fail "Start did not open: $(after "find #32768")"
[ "$(after "find #32770 Run")" = "found=1" ] && pass "Win+R opens Run" || fail "Run: $(after "find #32770 Run")"
[ "$(after 'find ExplorerWClass')" = "found=1" ] && pass "Win+E opens File Explorer" || fail "File Explorer: $(after 'find ExplorerWClass')"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
