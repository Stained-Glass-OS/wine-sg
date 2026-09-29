#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Right-click > Delete in File Explorer (patches/sg/0514). The folder view's
# "delete" verb asked "move to the Recycle Bin?" (FOF_ALLOWUNDO without
# FOF_NOCONFIRMATION), a prompt often hidden behind the window: the file
# stayed and nothing seemed to happen. As on Windows 10 it now asks only when
# "Display delete confirmation dialog" is on (SHELLSTATE.fNoConfirmRecycle
# clear) or a ConfirmFileDelete policy says so. The file goes to the Recycle
# Bin (the XDG trash), not away for good.
#
#   WINE=/opt/wine-sg/bin/wine test/recycle-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-recycle.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/recycle-probe.c" -lshell32 -lole32 -luuid -luser32 || { fail "probe did not build"; exit 1; }
# the item menu makes windows: a display of its own
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# a new profile: no ~/.local/share yet (the Recycle Bin must still work)
rm -rf "$XDG_DATA_HOME"
PUB="$WINEPREFIX/drive_c/users/Public"

out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 'C:\users\Public\rectest.txt' 2>/dev/null | tr -d '\r')
echo "      $out"
[ "$out" = "gone 1" ] && pass "right-click Delete removes the file, without a prompt (Windows 10's default)" \
    || fail "right-click Delete: '$out' (a hidden prompt blocks it)"
n=$(find "$HOME" -path '*Trash/files/rectest.txt' 2>/dev/null | wc -l)
[ "$n" -ge 1 ] && pass "it went to the Recycle Bin, not away for good" || fail "not in the trash ($(find "$HOME" -name 'rectest*' 2>/dev/null))"

# a ConfirmFileDelete policy asks again: without an answer the file stays
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer' /v ConfirmFileDelete /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 20 "$WINE" "$T/probe.exe" 'C:\users\Public\rectest2.txt' >/dev/null 2>&1
[ -f "$PUB/rectest2.txt" ] && pass "with the ConfirmFileDelete policy it asks first (not deleted unanswered)" \
    || fail "policy ignored: deleted without asking"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
