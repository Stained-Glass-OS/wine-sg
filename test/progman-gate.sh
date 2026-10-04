#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's desktop has Windows' desktop windows for programs to find
# (patches/sg/0799): "Progman" ("Program Manager") holding SHELLDLL_DefView
# and its SysListView32. Opera's installer looks for them through UI
# Automation and crashed without them (its shortcuts step, 2026-10-04 --
# with the root UI Automation gave it: GetRootElementBuildCache was a stub;
# the desktop had no children, elements no class name);
# wallpaper and desktop tools FindWindow("Progman") and send it 0x52C.
# They are one transparent, click-through pixel: the desktop draws itself.
#
#   WINE=/opt/wine-sg/bin/wine test/progman-gate.sh
# Mutants: SG_MUTANT_NO_PROGMAN (explorer), SG_MUTANT_UIA_STUBS,
# SG_MUTANT_UIA_NO_CHILDREN (uiautomationcore).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-progman.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/progman-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/uia.exe" "$HERE/uiadesktop-probe.c" -lole32 -loleaut32 -luuid || { fail "UIA probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
i=0; while [ $i -lt 60 ] && ! xdotool search --name 'shell - Wine Desktop' >/dev/null 2>&1; do sleep 0.5; i=$((i + 1)); done
sleep 3
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
[ "$(v PROGMAN)" = "yes Program Manager" ] && pass "FindWindow(\"Progman\"): \"Program Manager\"" || fail "Progman: '$(v PROGMAN)'"
[ "$(v CHAIN)" = "view list" ] && pass "its SHELLDLL_DefView holds a SysListView32" || fail "chain: '$(v CHAIN)'"
[ "$(v VISIBLE)" = "1 SIZE 1x1" ] && pass "shown, one transparent pixel" || fail "visible/size: '$(v VISIBLE)'"
[ "$(v WORKERW)" = 1 ] && pass "0x52C (a wallpaper tool's) is answered" || fail "0x52C: '$(v WORKERW)'"
# as Opera's installer finds them: UI Automation, the root with a cache, an
# And of class name and control type (GetRootElementBuildCache,
# CreateAndCondition and string/int property conditions were stubs)
timeout 60 "$WINE" "$T/uia.exe" 2>/dev/null | tr -d '\r' > "$T/u"
u() { sed -n "s/^$1 //p" "$T/u" | head -1; }
[ "$(u ROOT)" = "00000000 yes" ] && pass "UI Automation's root, with a cache request" || fail "root: '$(u ROOT)'"
[ "$(u AND)" = "00000000 yes" ] && pass "an And condition" || fail "And condition: '$(u AND)'"
[ "$(u PROGMAN)" = "00000000 found" ] && pass "the root's Progman pane found by class name and control type (the desktop's children, their class names)" || fail "Progman by UIA: '$(u PROGMAN)'"
[ "$(u CLASSNAME)" = "00000000 Progman" ] && pass "an element's class name read (get_CurrentClassName)" || fail "class name: '$(u CLASSNAME)'"
[ "$(u LIST)" = "00000000 found" ] && pass "its SysListView32 found below it" || fail "list by UIA: '$(u LIST)'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
