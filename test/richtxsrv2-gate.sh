#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RichEdit's ITextServices2 and ITextDocument2 (patches/sg/1490), under Xvfb:
# test/richtxsrv2-probe.c hosts windowless text services through msftedit's
# CreateTextServices, as Office's React Native text boxes do.  Word's start
# screen looked up IID_ITextServices2 in the RichEdit DLL, asked for
# ITextServices2 and ITextDocument2 and drew with TxDrawD2D; when any was
# missing Word ended in a fail-fast (0x02784198) as soon as it showed its
# start screen.
#
#  - IID_ITextServices2 is exported (msftedit and riched20);
#  - the text services answer ITextServices2 and ITextDocument2;
#  - ITextDocument2 keeps its notification mode and typography options;
#  - TxGetNaturalSize2 gives a size and the first line's ascent;
#  - TxDrawD2D draws the text into a Direct2D (WIC bitmap) target, inside the
#    bounds only, and leaves a transparent background transparent.
#
#   WINE=/opt/wine-sg/bin/wine test/richtxsrv2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${RICHTXSRV2_DPY:-287}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-richtxsrv2.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/richtxsrv2-probe.exe" "$HERE/richtxsrv2-probe.c" \
    -ld2d1 -lwindowscodecs -lole32 -luuid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/richtxsrv2-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check export_msftedit         "msftedit exports IID_ITextServices2"
check export_riched20         "riched20 exports IID_ITextServices2"
check create                  "CreateTextServices with a windowless host"
check qi_services2            "the text services answer ITextServices2"
check qi_document2            "and ITextDocument2"
check doc2_notification_mode  "ITextDocument2 keeps its notification mode"
check doc2_typography         "and its typography options"
check settext                 "TxSetText"
check natural_size2           "TxGetNaturalSize2 gives the text's size"
check ascent                  "and the first line's ascent"
check d2d_target              "a Direct2D WIC bitmap target"
check txdrawd2d               "TxDrawD2D draws (and EndDraw succeeds)"
check text_drawn              "the text is drawn, opaque"
check background_kept         "a transparent background stays transparent"
check bounds_kept             "nothing is drawn outside the bounds"
check done                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
