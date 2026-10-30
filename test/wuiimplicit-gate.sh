#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition ICompositor2 (patches/sg/6195), under Xvfb:
#
#  - the compositor answers ICompositor2; CreateImplicitAnimationCollection makes a map of
#    property names to animations (Insert tells a replacement; Lookup, HasKey, Remove, its
#    view and an iterator of its pairs);
#  - a visual answers ICompositionObject2 and keeps its ImplicitAnimations; setting its Offset
#    then runs the collection's "Offset" animation (key frame "this.FinalValue", Target "Offset"
#    through ICompositionAnimation2) from the old value to the new one; without a collection
#    the new value takes effect at once;
#  - CreateStepEasingFunction makes a step function; four steps with single-frame first and
#    last steps hold the third step late in an animation.
# WinUI 2's NavigationView (Windows Terminal's settings) sets such an animation on its
# overflow button while applying its template; without ICompositor2 it access-violated.
#
#   WINE=/opt/wine-sg/bin/wine test/wuiimplicit-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/dcomp): SG_MUTANT_NO_COMPOSITOR2, SG_MUTANT_NO_OBJECT2, SG_MUTANT_NO_IMPLICIT_RUN,
# SG_MUTANT_FINAL_IS_START, SG_MUTANT_STEP_NO_SINGLE_FRAME, SG_MUTANT_NO_ANIMATION2.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${WUIIMPLICIT_DPY:-262}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done

T=$(mktemp -d /var/tmp/sg-wuiimplicit.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/wuiimplicit-probe.exe" "$HERE/wuiimplicit-probe.c" \
    -lole32 -luuid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/wuiimplicit-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}

check 'compositor2=1'                        "the compositor answers ICompositor2"
check 'collection=1'                         "CreateImplicitAnimationCollection makes an ImplicitAnimationCollection"
check 'animation2=1'                         "a key frame animation answers ICompositionAnimation2"
check 'target=Offset'                        "and keeps its Target"
check 'insert=1 replaced=0'                  "Insert of a new name replaces nothing"
check 'again replaced=1 size=1 has=1 missing=0 lookup=1 lookup_missing=0x8000000b' "again it replaces; Size, HasKey, Lookup; a missing name is out of bounds"
check 'remove_missing=0x8000000b view_size=1' "removing a missing name is out of bounds; the view has the entry"
check 'iterator first=1 key=Offset next=0'   "the iterator gives the pair, then ends"
check 'object2=1'                            "a visual answers ICompositionObject2"
check 'implicit_kept=1'                      "it keeps its ImplicitAnimations"
check 'implicit_mid=1'                       "setting Offset runs the implicit animation: halfway it is between"
check 'implicit_end=100'                     "and it ends at the new value"
check 'no_implicit=20'                       "without a collection a new Offset takes effect at once"
check 'step=1 count=1 final=1'               "a step easing function: one step, final step 1"
check 'step_final_follows=4'                 "its final step follows StepCount"
check 'step_late=0.75 step_end=1.00'         "four steps, single-frame first and last: late, the third step; then the end"
check 'done=1'                               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
