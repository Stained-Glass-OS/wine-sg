#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The application-command keys (patches/sg/6524): a window that leaves VK_BROWSER_BACK..VK_LAUNCH_APP2
# to DefWindowProc gets WM_APPCOMMAND (FAPPCOMMAND_KEY), and a shell hook window in another process
# HSHELL_APPCOMMAND, as the media keys must reach the desktop's player controls; a window that
# handles WM_KEYDOWN itself makes none. Real X key events (xdotool) through winex11, whose
# XF86AudioPlay/Pause/Next/Prev/Stop, Calculator and MyComputer keysyms are checked too
# (test/appcmdkeys-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/appcmdkeys-gate.sh     (SG_APPCMD_DISPLAY=N: that display)
# Mutant: SG_MUTANT_APPCMD_NO_KEY (win32u defwnd.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=; HP=; TP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool xmodmap "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-appcmd.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; for p in $XP $HP $TP; do kill "$p" 2>/dev/null; done; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/appcmdkeys-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
if [ -n "${SG_APPCMD_DISPLAY:-}" ]; then
    Xvfb ":$SG_APPCMD_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$SG_APPCMD_DISPLAY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    echo "$SG_APPCMD_DISPLAY" > "$T/display"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# the keysyms on keycodes of their own (one keysym each), before Wine reads the keyboard
xmodmap -e "keycode 172 = XF86AudioPlay" -e "keycode 209 = XF86AudioPause" -e "keycode 171 = XF86AudioNext" \
        -e "keycode 173 = XF86AudioPrev" -e "keycode 174 = XF86AudioStop" -e "keycode 148 = XF86Calculator" \
        -e "keycode 165 = XF86MyComputer" -e "keycode 208 = NoSymbol" -e "keycode 215 = NoSymbol"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/"
"$WINESERVER" -p 60
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }
wait_line() { i=0; while ! tr -d '\r' < "$1" | grep -q "$2" && [ $i -lt 200 ]; do sleep 0.1; i=$((i + 1)); done; tr -d '\r' < "$1" | grep -q "$2"; }
lines() { tr -d '\r' < "$1" | grep -c "$2"; }

timeout -s KILL 200 "$WINE" 'C:\probe.exe' hook > "$T/hook" 2>/dev/null & HP=$!
wait_line "$T/hook" "^hook_ready=1" || { fail "the shell hook window"; cat "$T/hook"; exit 1; }
pass "a shell hook window in its own process"

# the window that leaves its keys to DefWindowProc
timeout -s KILL 200 "$WINE" 'C:\probe.exe' target > "$T/target" 2>/dev/null & TP=$!
wait_line "$T/target" "^target_ready=1" || { fail "the target window"; exit 1; }
WID=$(xdotool search --sync --name '^sgappcmd-target$' | head -n 1)
xdotool windowactivate --sync "$WID" 2>/dev/null || xdotool windowfocus --sync "$WID"
sleep 1
for k in XF86AudioPlay XF86AudioNext XF86AudioPause XF86AudioPrev XF86AudioStop XF86Calculator XF86MyComputer; do
    xdotool key --clearmodifiers "$k"; sleep 0.4
done
sleep 1
tr -d '\r' < "$T/target" | sed 's/^/      target: /'
tr -d '\r' < "$T/hook" | sed 's/^/      hook: /'
vk() { yes_no "$(tr -d '\r' < "$T/target" | grep -c "^key vk=$1$" | grep -q "^$2$" && echo 1)" "winex11: $3 is $4"; }
vk 0xb3 2 "XF86AudioPlay and XF86AudioPause" "VK_MEDIA_PLAY_PAUSE"
vk 0xb0 1 "XF86AudioNext" "VK_MEDIA_NEXT_TRACK"
vk 0xb1 1 "XF86AudioPrev" "VK_MEDIA_PREV_TRACK"
vk 0xb2 1 "XF86AudioStop" "VK_MEDIA_STOP"
vk 0xb7 1 "XF86Calculator" "VK_LAUNCH_APP2"
vk 0xb6 1 "XF86MyComputer" "VK_LAUNCH_APP1"
SELF=$(tr -d '\r' < "$T/target" | sed -n 's/^self_appcmd cmd=\([0-9]*\) device=0 window=1$/\1/p' | tr '\n' ' ')
yes_no "$([ "$SELF" = "14 11 14 12 13 18 17 " ] && echo 1)" "the window gets WM_APPCOMMAND (FAPPCOMMAND_KEY) for each, in the documented mapping" "$SELF"
HOOK=$(tr -d '\r' < "$T/hook" | sed -n 's/^hook_appcmd cmd=\([0-9]*\) device=0 keys=.*/\1/p' | tr '\n' ' ')
yes_no "$(printf '%s' "$HOOK" | grep -q "^14 11 " && echo 1)" "the shell hook gets HSHELL_APPCOMMAND MEDIA_PLAY_PAUSE, then MEDIA_NEXTTRACK" "$HOOK"
yes_no "$([ "$HOOK" = "14 11 14 12 13 18 17 " ] && echo 1)" "and every other key's command, once each" "$HOOK"

# the window that handles WM_KEYDOWN itself
BEFORE=$(lines "$T/hook" "^hook_appcmd")
timeout -s KILL 200 "$WINE" 'C:\probe.exe' target eat > "$T/eat" 2>/dev/null &
EP=$!
wait_line "$T/eat" "^target_ready=1" || { fail "the second target window"; exit 1; }
WID=$(xdotool search --sync --name '^sgappcmd-eat$' | head -n 1)
xdotool windowactivate --sync "$WID" 2>/dev/null || xdotool windowfocus --sync "$WID"
sleep 1
xdotool key --clearmodifiers XF86AudioPlay; sleep 0.4; xdotool key --clearmodifiers XF86AudioNext
sleep 1.5
yes_no "$([ "$(lines "$T/eat" "^key vk=")" = 2 ] && echo 1)" "the second window got the keys"
yes_no "$([ "$(lines "$T/eat" "^self_appcmd")" = 0 ] && [ "$(lines "$T/hook" "^hook_appcmd")" = "$BEFORE" ] && echo 1)" \
    "a window that handles WM_KEYDOWN makes no app command"

touch "$C/stop"
wait "$HP" "$TP" "$EP" 2>/dev/null; HP=; TP=
yes_no "$(tr -d '\r' < "$T/hook" | grep -q '^done=1' && echo 1)" "the hook window ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
