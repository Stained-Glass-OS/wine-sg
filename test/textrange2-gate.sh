#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RichEdit's ITextRange2, ITextSelection2 and ITextStory (patches/sg/1510),
# under Xvfb: test/textrange2-probe.c goes through a RichEdit's
# ITextDocument2 as Office's React Native text boxes do. Word's start screen
# box ("Describe the document you'd like to write") read and replaced its
# text through them; with ITextDocument2's Range2, GetSelection2 and
# GetMainStory stubs (1490), typing went after the placeholder instead of
# replacing it.
#
#  - GetMainStory: the main story, its type and text, SetText and GetRange;
#  - Range2: an ITextRange2 -- GetCch, GetText2, GetChar2, GetDuplicate2;
#  - GetSelection2: the selection as ITextSelection2, SetText2 replacing it.
#
#   WINE=/opt/wine-sg/bin/wine test/textrange2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_RICHED_NO_RANGE2 (a range does not answer ITextRange2),
# SG_MUTANT_RICHED_NO_STORY, SG_MUTANT_RICHED_SETTEXT2_NOOP (richole.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${TEXTRANGE2_DPY:-291}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-textrange2.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/textrange2-probe.exe" "$HERE/textrange2-probe.c" \
    -lole32 -loleaut32 -luser32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/textrange2-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
