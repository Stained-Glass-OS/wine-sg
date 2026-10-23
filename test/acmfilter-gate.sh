#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# acmFilterChooseW/A, the ACM filter chooser (patches/sg/6509; it was a stub): the drivers' filter
# tags and filters, the program's hook, INITTOFILTERSTRUCT, cancel, a too-small buffer, the ANSI
# form -- with a filter driver of the probe's own (test/acmfilter-probe.c), under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/acmfilter-gate.sh
# Mutant: SG_MUTANT_ACM_FILTER_NOT_RETURNED (msacm32 filter.c).
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
T=$(mktemp -d /var/tmp/sg-acmfilter.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/acmfilter-probe.c" -lmsacm32 -lwinmm -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check driver_added   "a filter driver of the probe's own is added"
check lists          "the dialog lists its filter tag and its two filters"
check chosen         "the chosen filter, its tag's and its own names are written back"
check init_to_struct "INITTOFILTERSTRUCT selects the program's filter"
check canceled       "Cancel is ACMERR_CANCELED"
check too_small      "a buffer too small: ACMERR_NOTPOSSIBLE and the size needed"
check ansi           "acmFilterChooseA"
check invalid        "an empty structure is MMSYSERR_INVALPARAM"
check done           "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
