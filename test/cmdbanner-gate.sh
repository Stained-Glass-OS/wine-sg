#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0440: cmd.exe's banner says Stained Glass OS, not
# "Microsoft Windows"; `ver` keeps the text batch scripts parse for the
# version; the console title is "Command Prompt", not Wine's.
#   WINE=... test/cmdbanner-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
W=$(mktemp -d /var/tmp/cmdbanner-gate.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() { set +e; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; rm -rf "$W"; }
trap cleanup EXIT
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1
out=$(printf 'ver\ntitle\nexit\n' | "$WINE" cmd 2>&1 | tr -d '\r')
head -1 <<<"$out" | grep -q '^Stained Glass OS \[Version 10\.' && pass "the banner: $(head -1 <<<"$out")" || fail "banner: $(head -1 <<<"$out")"
grep -q 'Microsoft Windows' <<<"$(head -2 <<<"$out")" && fail "the banner still says Microsoft Windows" || pass "the banner does not say Microsoft Windows"
grep -q '^Microsoft Windows 10\.' <<<"$out" && pass "ver keeps the text scripts parse" || fail "ver: $out"
printf '@echo off\r\nfor /f "tokens=3" %%%%v in (\x27ver\x27) do echo %%%%v\r\n' > "$W/ver.bat"
t=$(cd "$W" && "$WINE" cmd /c ver.bat 2>/dev/null | tr -d '\r')
[[ "$t" == 10.* ]] && pass "a script parsing ver still finds the version ($t)" || fail "ver parsing: $t"
exe=""
for c in "$(dirname "$WINE")/../lib/wine/x86_64-windows/cmd.exe" "$(dirname "$WINE")/programs/cmd/x86_64-windows/cmd.exe"; do [ -f "$c" ] && exe=$c; done
if [ -z "$exe" ]; then fail "cmd.exe not found next to $WINE"
elif strings -el "$exe" | grep -qx 'Wine Command Prompt'; then fail "cmd.exe still carries Wine's console title"
else pass "the console title is not Wine's"; fi
echo "cmdbanner-gate: $fails failure(s)"
[ "$fails" = 0 ]
