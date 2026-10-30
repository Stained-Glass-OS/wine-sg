#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.ApplicationModel (patches/sg/6196): CoreApplication's current view answers
# ICoreApplicationView3 (its TitleBar: no system title bar to measure, ExtendViewIntoTitleBar kept,
# handlers taken, a weak reference that resolves) and ICoreApplicationView5 (its own Properties);
# DesignMode says FALSE twice. WinUI 2's NavigationView (Windows Terminal's settings) takes the
# title bar's metrics while applying its template, ItemsRepeater asks DesignMode while measuring,
# and both access-violated or failed without them.
#
#   WINE=/opt/wine-sg/bin/wine test/appview3-gate.sh
# Mutants (dlls/windows.applicationmodel): SG_MUTANT_NO_VIEW3, SG_MUTANT_NO_TITLE_BAR_WEAKREF,
# SG_MUTANT_NO_VIEW5, SG_MUTANT_NO_DESIGN_MODE.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-appview3.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/appview3-probe.exe" "$HERE/appview3-probe.c" -lole32 -luuid -lruntimeobject \
    || { fail "probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
unset DISPLAY WAYLAND_DISPLAY
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/appview3-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\$"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%[= ]*}" || echo none))"; fi
}

check 'view3=1'                                          "the current view answers ICoreApplicationView3"
check 'component=0 title_bar=1 same=1 closing_handler=1' "not a component; its TitleBar, the same each time; HostedViewClosing takes a handler"
check 'bar height=0 insets=0,0 visible=0'                "the title bar: nothing to measure, not visible"
check 'extend=1 metrics_handler=1'                       "ExtendViewIntoTitleBar is kept; LayoutMetricsChanged takes a handler"
check 'weak=1'                                           "its weak reference resolves to it"
check 'view5=1'                                          "the current view answers ICoreApplicationView5"
check 'view_properties=1 same=1 not_app=1'               "its Properties: one set, its own"
check 'design_mode=0 design_mode2=0'                     "DesignMode: not in a designer"
check 'done=1'                                           "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
