#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UI Automation's control view walks (patches/sg/1472): a tree walker with a
# condition -- the control view, the content view -- went to no child or
# sibling (E_NOTIMPL: only the raw "true" view navigated), so an
# accessibility client walking the desktop (a screen reader, Accessibility
# Insights, Opera's installer) stopped at the root. Now an element the view
# leaves out is seen through, as on Windows. test/uiaview-probe.c walks the
# shell's desktop with Notepad open (another process), forwards and back.
#
#   WINE=/opt/wine-sg/bin/wine test/uiaview-gate.sh
# The taskbar's buttons have names (1473): Start, and a program's button its
# window's title -- they were nameless, a screen reader's "button".
# Mutants: SG_MUTANT_UIA_NO_VIEW_NAV (uiautomationcore uia_client.c),
# SG_MUTANT_TASKBAR_NAMELESS (explorer systray.c).
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
T=$(mktemp -d /var/tmp/sg-uiaview.XXXXXX)
unset WAYLAND_DISPLAY
mkdir -p "$T/xdg"; chmod 700 "$T/xdg"; export XDG_RUNTIME_DIR="$T/xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/walk.exe" "$HERE/uiaview-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
i=0; while [ $i -lt 60 ] && ! xdotool search --name 'shell - Wine Desktop' >/dev/null 2>&1; do sleep 0.5; i=$((i + 1)); done
sleep 3
"$WINE" notepad >/dev/null 2>&1 &
i=0; while [ $i -lt 60 ] && ! xdotool search --name 'Notepad' >/dev/null 2>&1; do sleep 0.5; i=$((i + 1)); done
sleep 2

timeout 120 "$WINE" "$T/walk.exe" 3 2>/dev/null | tr -d '\r' > "$T/fwd"
timeout 120 "$WINE" "$T/walk.exe" 3 reverse 2>/dev/null | tr -d '\r' > "$T/rev"
sed 's/^/      /' "$T/fwd"
grep -q '^done' "$T/fwd" && ! grep -q '^STALL\|(hr ' "$T/fwd" && pass "the control view walks to its end" \
    || fail "the walk: $(grep '^STALL\|(hr ' "$T/fwd" | head -2 | tr '\n' ' ')$(grep -q '^done' "$T/fwd" || echo ' (not done)')"
tray=$(sed -n 's/^  \[.*\] class=Shell_TrayWnd .* pid=\([0-9]*\)$/\1/p' "$T/fwd")
note=$(sed -n 's/^  \[.*Notepad\] class=Notepad .* pid=\([0-9]*\)$/\1/p' "$T/fwd")
[ -n "$tray" ] && grep -q '^  \[Program Manager\] class=Progman ' "$T/fwd" \
    && pass "the desktop's children: the taskbar, Progman" || fail "the desktop's children: $(grep '^  \[' "$T/fwd" | cut -c1-50 | tr '\n' ' ')"
[ -n "$note" ] && [ "$note" != "$tray" ] && pass "and Notepad's window, another process's ($note)" || fail "Notepad's window: '${note:-none}'"
sed -n '/class=Notepad /,/^  \[/p' "$T/fwd" | grep -q '^    \[.*\] class=' && pass "Notepad's own children, across the process" \
    || fail "no children under Notepad"
sed -n '/class=Progman /,/^  \[/p' "$T/fwd" | grep -q '^      \[FolderView\] class=SysListView32 ' \
    && pass "three deep: Progman, SHELLDLL_DefView, the SysListView32" || fail "under Progman: $(sed -n '/class=Progman /,$p' "$T/fwd" | head -4 | tr '\n' ' ')"
grep -q '^done' "$T/rev" && [ "$(grep '^ ' "$T/fwd" | sed 's/ hwnd=.*//' | sort)" = "$(grep '^ ' "$T/rev" | sed 's/ hwnd=.*//' | sort)" ] \
    && pass "backwards (last child, previous sibling) the same elements" \
    || fail "backwards: $(grep -c '^ ' "$T/rev") elements, forwards $(grep -c '^ ' "$T/fwd")"
ntitle=$(sed -n 's/^  \[\(.*Notepad\)\] class=Notepad .*/\1/p' "$T/fwd" | head -1)
sed -n '/class=Shell_TrayWnd /,/^  \[/p' "$T/fwd" | grep -q '^    \[Start\] class=Button ' \
    && pass "the taskbar's Start button is named Start" || fail "the Start button's name: $(sed -n '/class=Shell_TrayWnd /,/^  \[/p' "$T/fwd" | grep -m1 'class=Button' | cut -c1-40)"
[ -n "$ntitle" ] && sed -n '/class=Shell_TrayWnd /,/^  \[/p' "$T/fwd" | grep -qF "    [$ntitle] class=Button " \
    && pass "Notepad's taskbar button is named its window's title ($ntitle)" \
    || fail "Notepad's taskbar button: $(sed -n '/class=Shell_TrayWnd /,/^  \[/p' "$T/fwd" | grep 'class=Button' | cut -c1-40 | tr '\n' ' ')"
# a view that leaves out the taskbar and SHELLDLL_DefView: their children
# are seen through, in their places
timeout 120 "$WINE" "$T/walk.exe" 3 skip:Shell_TrayWnd 2>/dev/null | tr -d '\r' > "$T/notray"
timeout 120 "$WINE" "$T/walk.exe" 3 skip:Shell_TrayWnd reverse 2>/dev/null | tr -d '\r' > "$T/notray-rev"
timeout 120 "$WINE" "$T/walk.exe" 3 skip:SHELLDLL_DefView 2>/dev/null | tr -d '\r' > "$T/noview"
sed 's/^/      /' "$T/notray"
! grep -q 'class=Shell_TrayWnd' "$T/notray" && grep -q '^  \[\] class=SgVirtualDesktopPager ' "$T/notray" \
    && pass "a view leaving out the taskbar: its children are the desktop's" \
    || fail "without the taskbar: $(grep '^  \[' "$T/notray" | cut -c1-40 | tr '\n' ' ')"
[ "$(grep '^  \[' "$T/notray" | grep -v 'class=Button \|class=SgVirtualDesktopPager ' | sed 's/ hwnd=.*//')" = \
  "$(grep '^  \[' "$T/fwd" | grep -v 'class=Shell_TrayWnd ' | sed 's/ hwnd=.*//')" ] && grep -q '^done' "$T/notray" \
    && pass "and after them, the taskbar's own next siblings (past a left-out parent's end)" \
    || fail "the siblings after the taskbar's children: $(grep '^  \[' "$T/notray" | sed 's/ hwnd=.*//' | tr '\n' ' ')"
grep -q '^done' "$T/notray-rev" && [ "$(grep '^ ' "$T/notray" | sed 's/ hwnd=.*//' | sort)" = "$(grep '^ ' "$T/notray-rev" | sed 's/ hwnd=.*//' | sort)" ] \
    && pass "and backwards, the same" || fail "without the taskbar backwards: $(grep '^  \[' "$T/notray-rev" | sed 's/ hwnd=.*//' | tr '\n' ' ')"
sed -n '/class=Progman /,/^  \[/p' "$T/noview" | grep -q '^    \[FolderView\] class=SysListView32 ' \
    && ! grep -q 'class=SHELLDLL_DefView' "$T/noview" && pass "a view leaving out SHELLDLL_DefView: the SysListView32 is Progman's child" \
    || fail "without SHELLDLL_DefView: $(sed -n '/class=Progman /,$p' "$T/noview" | head -3 | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
