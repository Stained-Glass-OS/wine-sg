#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UI Automation client pattern objects and UiaSetFocus (patches/sg/2610):
# test/uiapat-probe.c drives every Xxx_Method export through a node whose
# provider logs the calls it gets.
#
#   WINE=/opt/wine-sg/bin/wine test/uiapat-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (uiautomationcore): SG_MUTANT_UIAPAT_ARGS_SWAPPED (uia_patterns.c,
# the two numbers of Scroll/Move/Resize/SetScrollPercent arrive swapped),
# SG_MUTANT_UIAPAT_NO_TYPE_CHECK (uia_patterns.c, a pattern object is accepted
# by the methods of any pattern), SG_MUTANT_UIAPAT_ERROR_DROPPED (uia_patterns.c,
# the provider's HRESULT is replaced by S_OK), SG_MUTANT_UIAPAT_UNSUPPORTED_OBJECT
# (uia_patterns.c, a pattern the provider lacks still yields an object),
# SG_MUTANT_UIAPAT_NO_FRAGMENT_CHECK (uia_patterns.c, UiaSetFocus on a provider
# without the fragment interface succeeds), SG_MUTANT_UIAPAT_NO_ACC_OUT (uia_patterns.c,
# GetIAccessible returns no object), SG_MUTANT_MSAA_RECT_ORDER (uia_provider.c,
# BoundingRectangle width and height swapped), SG_MUTANT_MSAA_PID_ZERO (ProcessId 0),
# SG_MUTANT_MSAA_LEGACY_STATE_AS_ROLE (the Legacy Role property reads the state),
# SG_MUTANT_MSAA_CLASS_FOR_CHILD (a child id answers with the window class).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${UIAPAT_DPY:-243}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-uiapat.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/uiapat-probe.exe" "$HERE/uiapat-probe.c" \
    -lole32 -loleaut32 -luiautomationcore -loleacc -luuid -luser32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/uiapat-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
