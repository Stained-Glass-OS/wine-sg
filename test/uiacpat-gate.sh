#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# the COM pattern objects of IUIAutomationElement::GetCurrentPattern(As) (patches/sg/2632):
# test/uiacpat-probe.c against a logging provider, under Xvfb.
#
#   WINE=/opt/wine-sg/bin/wine test/uiacpat-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (uiautomationcore, uia_com_client.c): SG_MUTANT_UIACPAT_ARGS_SWAPPED, SG_MUTANT_UIACPAT_UNSUPPORTED_OBJECT,
# SG_MUTANT_UIACPAT_ANY_IID, SG_MUTANT_UIACPAT_ERROR_DROPPED, SG_MUTANT_UIACPAT_WRONG_PROP.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${UIACPAT_DPY:-246}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-uiacpat.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -mwindows $inc -o "$T/uiacpat-probe.exe" "$HERE/uiacpat-probe.c" \
        -lole32 -loleaut32 -luuid -luser32 -luiautomationcore 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/uiacpat-probe.exe" >"$T/out.raw" 2>"$T/err" </dev/null; if [ -n "${GATE_KEEPERR:-}" ]; then tail -30 "$T/err" >&2; fi
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
