#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Pasted into its own folder, a copy beside it named as Windows names it
# (patches/sg/1176): "a.txt" -> "a - Copy.txt", then "a - Copy (2).txt"; a
# folder "F" -> "F - Copy" with what is in it -- nothing asked. File
# Explorer's Ctrl+C, Ctrl+V asked Replace, Skip or Keep both (and "Keep
# both" made "a (2).txt"; the regression walk, 2026-10-05).
#
# The probe copies with SHFileOperation (no FOF_NOCONFIRMATION): a question
# would be a dialog nobody answers, and the probe's time runs out.
#
#   WINE=/opt/wine-sg/bin/wine test/selfcopy-gate.sh   (mutant SG_MUTANT_SELF_COPY_ASKS)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-selfcopy.XXXXXX)
n=210; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$n" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER \
       WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O1 -municode -o "$T/selfcopy-probe.exe" "$HERE/selfcopy-probe.c" -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
D="$WINEPREFIX/drive_c/work"
mkdir -p "$D/F/sub"
printf 'hello' > "$D/a.txt"; printf 'x' > "$D/F/sub/in.txt"
cp "$T/selfcopy-probe.exe" "$WINEPREFIX/drive_c/"
probe() { timeout -s KILL 30 "$WINE" 'C:\selfcopy-probe.exe' 'C:\work' "$@" 2>/dev/null | tr -d '\r'; }

out=$(probe a.txt)
[ -f "$D/a - Copy.txt" ] && [ "$(cat "$D/a - Copy.txt")" = hello ] && pass "a file pasted into its own folder: \"a - Copy.txt\", nothing asked" \
    || fail "first copy: $out; $(ls "$D" | tr '\n' ' ')"
out=$(probe a.txt)
[ -f "$D/a - Copy (2).txt" ] && pass "again: \"a - Copy (2).txt\"" || fail "second copy: $out; $(ls "$D" | tr '\n' ' ')"
out=$(probe F)
[ -f "$D/F - Copy/sub/in.txt" ] && pass "a folder: \"F - Copy\" with what is in it" || fail "folder copy: $out; $(ls -R "$D" | tr '\n' ' ')"
[ "$(cat "$D/a.txt")" = hello ] && [ -f "$D/F/sub/in.txt" ] && pass "the originals are as they were" || fail "originals changed"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
