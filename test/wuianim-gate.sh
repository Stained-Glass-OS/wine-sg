#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition property sets, easing functions, key frame
# animations and batches (patches/sg/6048), under Xvfb:
#
#  - property sets keep values by name and type, telling a missing name
#    from one of another type; an object's Properties is one set;
#  - cubic bezier easing functions keep their control points and refuse
#    curves that go back in time; key frame animations keep their key
#    frames, duration and stop behaviour; expression animations their
#    expression and parameters;
#  - a Vector3 key frame animation started on a sprite visual's Offset
#    moves it over its second (halfway, it is between), the window shows the
#    sprite where it ended, and the scoped batch it was started in tells its
#    Completed handler -- subscribed after End, as programs do -- once, after
#    the animation finished, with the batch as sender;
#  - "Offset.Y" animates one component, "Color" a colour brush (the window
#    turns green); an animation of the wrong type is refused; an ease-in
#    curve lags behind linear progress;
#  - StopAnimation leaves the initial or the final value, as the stop
#    behaviour says;
#  - an empty batch completes, a suspended batch leaves out what is started
#    meanwhile, and the commit batch completes after the next commit.
#
#   WINE=/opt/wine-sg/bin/wine test/wuianim-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${WUIANIM_DPY:-687}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wuianim.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/wuianim-probe.exe" "$HERE/wuianim-probe.c" \
    -lole32 -luuid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/wuianim-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}

check 'compositor_activated=1'  "Windows.UI.Composition.Compositor activates"
check 'propset_created=1'       "CreatePropertySet makes a property set"
check 'propset_scalar=1'        "it keeps a scalar by name"
check 'propset_mismatch=1'      "asked for another type, it says TypeMismatch"
check 'propset_notfound=1'      "asked for a name it lacks, NotFound"
check 'propset_replace=1'       "inserting a name again replaces its value and type"
check 'propset_boolean=1'       "ICompositionPropertySet2 keeps booleans"
check 'object_properties=1'     "a visual's Properties is one set, the same each time"
check 'easing_cubic=1'          "a cubic bezier easing function keeps its control points, refuses x outside 0-1"
check 'keyframe_object=1'       "key frame animations keep key frames by key, their duration, refuse bad keys and durations"
check 'expression_object=1'     "expression animations keep their expression and parameters"
check 'start_animation=1'       "StartAnimation takes a Vector3 animation for Offset"
check 'anim_midway=1'           "halfway through, the visual is between where it was and where it goes"
check 'batch_completed=1'       "the scoped batch tells its Completed handler once"
check 'batch_after_animation=1' "only after the animation finished"
check 'batch_sender=1'          "with the batch as sender and composition event arguments"
check 'batch_state=1'           "an ended batch is IsEnded and not IsActive"
check 'anim_final=1'            "the animation leaves its final value"
check 'anim_drawn=1'            "and the window shows the sprite where it ended"
check 'anim_component=1'        "\"Offset.Y\" animates one component"
check 'anim_color=1'            "a colour key frame animation turns a brush green"
check 'anim_typecheck=1'        "an animation of the wrong type is refused, a property set entry is animated"
check 'anim_eased=1'            "an ease-in curve lags behind linear progress"
check 'stop_initial=1'          "StopAnimation sets the initial value when asked to"
check 'stop_final=1'            "and the final value by default"
check 'empty_batch=1'           "an empty scoped batch completes"
check 'suspended_batch=1'       "a suspended batch leaves out what is started meanwhile"
check 'commit_batch=1'          "the commit batch completes after the next commit"
check 'done=1'                  "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
