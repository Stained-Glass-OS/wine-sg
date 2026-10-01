#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The wineserver puts a saved registry hive on the disk before it replaces
# the old one (patches/sg/0635). It wrote the hive to a temporary file and
# renamed that over system.reg without fsync: after a power loss the rename
# could be on the disk and the data not, and a QA machine hard-reset by its
# host came back with an 18 MB system.reg of zeros -- all of HKLM gone, and
# the login screen with it ("is a 32-bit installation"). Under strace, the
# gate requires that the temporary file is fsync'd before it is renamed to
# system.reg, that the folder is fsync'd after the rename, and that the hive
# holds a value just written.
#
#   WINE=/opt/wine-sg/bin/wine test/regfsync-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v strace >/dev/null || { echo "SKIP: strace not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-regfsync.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# a traced server, a value in HKLM, and the save at shutdown
strace -f -y -o "$T/st.log" -e trace=fsync,fdatasync,rename,renameat,renameat2 "$WINESERVER" -f -p5 &
sleep 2
timeout 60 "$WINE" reg add 'HKLM\Software\SgRegFsync' /v Probe /d durable /f >/dev/null 2>&1
"$WINESERVER" -k
wait
grep -q 'Probe"="durable"' "$WINEPREFIX/system.reg" && pass "the value is in the saved system.reg" ||
    fail "system.reg lacks the value"
# the rename of a temporary hive onto system.reg, and what came before/after
python3 - "$T/st.log" <<'EOF' > "$T/order.txt"
import re, sys
lines = open(sys.argv[1]).read().splitlines()
for n, l in enumerate(lines):
    m = re.search(r'rename(?:at2?)?\(.*?"(reg[0-9a-f]+\.tmp)".*?"system\.reg"', l)
    if not m: continue
    tmp = m.group(1)
    before = any(re.search(r'f(data)?sync\(\d+<[^>]*/' + re.escape(tmp) + '>', x) for x in lines[:n])
    after = any(re.search(r'fsync\(\d+<[^>]*/prefix>\)', x) for x in lines[n + 1:])
    print("rename", tmp, "synced_before=%d dir_synced_after=%d" % (before, after))
EOF
cat "$T/order.txt" | sed 's/^/      /'
grep -q '^rename' "$T/order.txt" || { fail "no rename of a temporary hive onto system.reg seen"; exit 1; }
grep -q 'synced_before=1' "$T/order.txt" && ! grep -q 'synced_before=0' "$T/order.txt" &&
    pass "the temporary hive is fsync'd before it replaces system.reg" ||
    fail "system.reg replaced by a hive not fsync'd"
grep -q 'dir_synced_after=1' "$T/order.txt" && pass "and the folder after the rename" ||
    fail "the folder was not fsync'd after the rename"
exit $RC
