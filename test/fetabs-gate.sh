#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer's tabs (patches/sg/0774; David 2026-10-02: "the file
# explorer needs to be able to have tabs"):
#   1. a window opens with one tab, named for its folder
#   2. Ctrl+T opens a second, which goes where the address bar sends it
#   3. Ctrl+Tab goes back to the first: the window shows its folder again
#   4. each tab has its own back: Alt+Left in the second goes to where the
#      second was, not to the first tab's folder
#   5. Ctrl+W closes a tab and the window stays; the last tab, the window
# Mutant SG_MUTANT_TABS_CLOSE_WINDOW (Ctrl+W closes the window) fails it.
#
#   WINE=/opt/wine-sg/bin/wine test/fetabs-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run xdotool; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fetabs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/explorer-probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/TabOne/Deeper" "$WINEPREFIX/drive_c/TabTwo/Inner"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" explorer-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
go() { xdotool key ctrl+l; sleep 0.5; xdotool key ctrl+a; xdotool type --delay 20 "\$1"; xdotool key Return; }
"$WINE" explorer /desktop=shell,1280x800 > /dev/null 2>&1 &
sleep 6
"$WINE" explorer 'C:\\TabOne' > /dev/null 2>&1 &
P wait-title TabOne 20 > /dev/null; sleep 2
P tabs | grep tabs= > "$T/t1"
xdotool key ctrl+t; sleep 2
go 'C:\\TabTwo'; P wait-title TabTwo 10 > /dev/null; sleep 1
go 'C:\\TabTwo\\Inner'; P wait-title Inner 10 > /dev/null; sleep 1
P tabs | grep tabs= > "$T/t2"; import -window root "$T/tabs.png" 2>/dev/null
xdotool key ctrl+Tab; sleep 2
P title > "$T/title3"; P tabs | grep tabs= > "$T/t3"
go 'C:\\TabOne\\Deeper'; P wait-title Deeper 10 > /dev/null; sleep 1
xdotool key ctrl+Tab; sleep 2
xdotool key alt+Left; sleep 2
P title > "$T/title4"
xdotool key ctrl+w; sleep 2
P tabs | grep tabs= > "$T/t5"; P find ExplorerWClass > "$T/find5"
xdotool key ctrl+w; sleep 2
P find ExplorerWClass > "$T/find6"
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 1280x800x24" "$T/session.sh" > "$T/session.out" 2>&1
c() { cat "$T/$1" 2>/dev/null; }
[ -n "${ARTIFACTS:-}" ] && cp "$T/tabs.png" "$ARTIFACTS/fetabs.png" 2>/dev/null
echo "      $(c t1) / $(c t2) / $(c t3) / $(c title3) / $(c title4) / $(c t5) $(c find5) / $(c find6)"
case "$(c t1)" in "tabs=1 current=1: TabOne") pass "a window opens with one tab, named for its folder" ;; *) fail "first: $(c t1)" ;; esac
case "$(c t2)" in "tabs=2 current=2: TabOne | Inner") pass "Ctrl+T opens a second tab, and it goes where it is sent" ;; *) fail "second: $(c t2)" ;; esac
case "$(c t3)" in "tabs=2 current=1:"*) grep -q 'title=TabOne' "$T/title3" && pass "Ctrl+Tab goes back to the first: its folder shows" || fail "back to the first: $(c title3)" ;; *) fail "Ctrl+Tab: $(c t3)" ;; esac
grep -q 'title=TabTwo' "$T/title4" && pass "each tab its own back: Alt+Left in the second goes to its own last folder" || fail "back in the second tab: $(c title4)"
case "$(c t5)" in "tabs=1 current=1:"*) grep -q 'found=1' "$T/find5" && pass "Ctrl+W closes the tab; the window stays" || fail "the window went with a tab" ;; *) fail "Ctrl+W: $(c t5) $(c find5)" ;; esac
grep -q 'found=0' "$T/find6" && pass "Ctrl+W on the last tab closes the window" || fail "the last tab: $(c find6)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
