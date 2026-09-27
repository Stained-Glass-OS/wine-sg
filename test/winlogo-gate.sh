#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0441: the default window icon (IDI_WINLOGO, user32's
# OIC_WINLOGO) is our four-diamond mark, not Wine's wine glass. Reads the
# icon out of the built user32.dll: purple at the top diamond, orange at the
# bottom one (the glass is red wine in a clear glass on transparency).
#   WINE=... test/winlogo-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in wrestool convert; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
dll=""
for c in "$(dirname "$WINE")/../lib/wine/x86_64-windows/user32.dll" "$(dirname "$WINE")/dlls/user32/x86_64-windows/user32.dll"; do
    [ -f "$c" ] && dll=$c; done
[ -n "$dll" ] || { echo "SKIP: no user32.dll next to $WINE"; exit 77; }
W=$(mktemp -d /var/tmp/winlogo-gate.XXXXXX); trap 'rm -rf "$W"' EXIT
wrestool -x --type=14 --name=32517 "$dll" > "$W/logo.ico" 2>/dev/null
[ -s "$W/logo.ico" ] && pass "user32 has OIC_WINLOGO" || { fail "no OIC_WINLOGO in $dll"; exit 1; }
px() { convert "$W/logo.ico[$1]" -format "%[pixel:p{$2,$3}]" info: 2>/dev/null | head -1; }
idx=$(identify "$W/logo.ico" 2>/dev/null | awk '/ 32x32 /{print NR-1; exit}')
[ -n "$idx" ] || { fail "no 32x32 image"; exit 1; }
top=$(px "$idx" 16 6); bottom=$(px "$idx" 16 25)
echo "      top $top, bottom $bottom"
python3 - "$top" "$bottom" <<'PY' && pass "it is our mark: purple on top, orange below" || fail "it is not our mark"
import re, sys
def rgb(s):
    n = [float(x) for x in re.findall(r'[\d.]+', s)[:3]]
    return n
t, b = rgb(sys.argv[1]), rgb(sys.argv[2])
ok = t[2] > 150 and t[0] < 160 and t[1] < 110      # purple 7B3FD0
ok = ok and b[0] > 200 and 110 < b[1] < 200 and b[2] < 100   # orange F0A030
sys.exit(0 if ok else 1)
PY
echo "winlogo-gate: $fails failure(s)"
[ "$fails" = 0 ]
