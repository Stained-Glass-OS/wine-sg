#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 6095: with echo on, a batch file's labels and "::"
# comments are not echoed (npm.cmd's ":: Created by npm" line showed with
# a prompt in front of it); commands and "rem" lines still are, and goto
# still finds the labels.
#   WINE=... test/cmdlabelecho-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
W=$(mktemp -d /var/tmp/cmdlabelecho-gate.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() { set +e; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; rm -rf "$W"; }
trap cleanup EXIT
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1
mkdir -p "$W/prefix/drive_c/work"
printf ':: Created by a tool, do not edit\r\n  :indented\r\necho one\r\ngoto :skip\r\necho skipped\r\n:skip\r\nrem a remark\r\necho two\r\n' > "$W/prefix/drive_c/work/t.bat"
out=$(cd "$W/prefix/drive_c/work" && "$WINE" cmd /c t.bat 2>&1 | tr -d '\r')
grep -q 'Created by a tool' <<<"$out" && fail "the :: comment is echoed: $out" || pass "the :: comment is not echoed"
grep -q ':indented\|>:skip' <<<"$out" && fail "a label is echoed: $out" || pass "labels are not echoed"
grep -q '^C:\\work>echo one *$' <<<"$out" && pass "commands are still echoed" || fail "command echo: $out"
grep -q '^C:\\work>rem a remark *$' <<<"$out" && pass "rem lines are still echoed" || fail "rem echo: $out"
grep -q '^skipped' <<<"$out" && fail "goto did not jump: $out" || { grep -qx two <<<"$out" && pass "goto still finds the label" || fail "goto: $out"; }
echo "cmdlabelecho-gate: $fails failure(s)"
[ "$fails" = 0 ]
