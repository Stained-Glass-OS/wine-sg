#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Claude Code in a Terminal tab (patches/sg/0794; David 2026-10-04: "the
# text and fonts were jumbled", "everything typed had an underline",
# "after exiting Claude the command line wasn't usable"). On a pseudo
# console, as a Terminal tab hosts it:
#   - xterm's private forms (CSI > 4;2 m, CSI > 5 u, CSI < u) are not SGR 4;2
#     (underlined, dim) or "restore the cursor" (jumbled drawing);
#   - SI (0x0F) is not drawn;
#   - light grey on green, then light grey on black: the green ends in the
#     tab (it ran on over the rest of the line and the next);
#   - Ctrl+C in raw mode is a key the program reads -- it ended the
#     console's reading of keys (the tab took no typing, or closed);
#   - a line read left waiting by a process that ended does not take the
#     next line typed: the next reader (the shell) gets it.
#
#   WINE=/opt/wine-sg/bin/wine test/claudecon-gate.sh
# Mutants: SG_MUTANT_VT_PRIVATE_AS_SGR, SG_MUTANT_VT_PRINTS_C0,
# SG_MUTANT_TTY_BG_STICKS, SG_MUTANT_CTRLC_ENDS_INPUT (conhost),
# SG_MUTANT_DEAD_READ_EATS_INPUT (wineserver).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-claudecon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/claudecon-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe.exe" "$WINEPREFIX/drive_c/claudecon.exe"
timeout 180 "$WINE" 'C:\claudecon.exe' host 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
[ "$(v VTCELL)" = "X 0007" ] && pass "CSI > 4;2 m, CSI > 5 u, CSI < u: no underline, the cursor where it was (X 0007)" \
    || fail "private forms: '$(v VTCELL)' (want X at its place, attribute 0007)"
[ "$(v VTSI)" = AB ] && pass "SI is not drawn (AB)" || fail "SI: '$(v VTSI)'"
[ "$(v VTRESET)" = yes ] && pass "grey on green back to grey: the green ends in the tab" || fail "colour back to grey: '$(v VTRESET)'"
[ "$(v RAW)" = key ] && pass "Ctrl+C in raw mode reaches the program as a key; the console goes on" || fail "raw Ctrl+C: '$(v RAW)'"
[ "$(v ALIVE)" = yes ] && pass "the console takes another program after it" || fail "after Ctrl+C: '$(v ALIVE)'"
[ "$(v LINE)" = hello ] && pass "a read left by a process that ended does not take the next line: the next reader gets it" \
    || fail "line after a dead read: '$(v LINE)'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
