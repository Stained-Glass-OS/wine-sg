#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Keyboard cues (patches/sg/6541): a new profile hides access-key underlines and focus rectangles
# until the keyboard is used, as Windows does by default (UserPreferencesMask without the 0x20 bit),
# and the keyboard shows them: Alt clears UISF_HIDEACCEL (WM_CHANGEUISTATE up to the top-level
# window, WM_UPDATEUISTATE down to every control), Tab in a dialog the focus rectangle; buttons,
# labels and menus draw their underlines by that state (DrawText DT_HIDEPREFIX), with the classic
# controls and with common controls 6; "Underline access keys" on shows them from the start
# (test/kbdcues-probe.c). Its own X server: the keys go through SendInput.
#   WINE=/opt/wine-sg/bin/wine test/kbdcues-gate.sh
# Mutants: SG_MUTANT_CUES_DEFAULT_ON (dlls/win32u/sysparams.c: the old 0x30 default),
#          SG_MUTANT_UISTATE_NONE (dlls/win32u/defwnd.c: WM_QUERYUISTATE always 0).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdcues.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/kbdcues-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
for mode in classic v6 cueson; do
    arg=; [ $mode != classic ] && arg=$mode
    out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' $arg 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $mode: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$mode: $2"
        else fail "$mode: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check cues_default_off "a new profile has keyboard cues off (Windows' default)"
    if [ $mode = cueson ]; then
        check cueson_shown "with \"Underline access keys\" on, a new window shows them"
    else
        [ $mode = v6 ] && check v6_context "common controls 6 active"
        check initial_hidden   "a new dialog hides the underlines and the focus rectangle"
        check alt_reveals      "Alt shows the access keys (WM_UPDATEUISTATE reaches the button)"
        check button_underline "the button draws its underline after Alt"
        check label_underline  "the label draws its underline after Alt"
        check tab_shows_focus  "Tab shows the focus rectangle"
        check menu_underline   "the menu bar draws its underlines once the keyboard is in use"
        check keyboard_opened_shown "a window opened after keyboard input shows its access keys"
    fi
    check done "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
