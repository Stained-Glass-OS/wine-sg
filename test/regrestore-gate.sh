#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RegRestoreKey restores a saved hive over a key (patches/sg/0506), as
# Windows does: the key's values and subkeys are replaced by the hive's
# root key's. Click-to-Run publishes Office's virtual registry (its MSI
# component paths, COM classes) with it; Wine's stub did nothing and
# reported success, and Office could not find its own components. A binary
# hive ("regf", written here by test/regf-write.py: compressed and UTF-16
# names, lh/li/ri subkey lists, inline, cell and big-data values) and a file
# RegSaveKey wrote (Wine's text format) both restore; a file that is not a
# hive is refused.
#
#   WINE=/opt/wine-sg/bin/wine test/regrestore-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-regrestore.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/regrestore-probe.c" || { fail "probe did not build"; exit 1; }
python3 "$HERE/regf-write.py" "$T/test.hiv" || { fail "no hive"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/test.hiv" "$WINEPREFIX/drive_c/test.hiv"
"$WINE" "$T/probe.exe" 'C:\test.hiv' 'C:\saved.reg' 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out" | head -40
v() { sed -n "s/^$1 //p" "$T/out"; }
# the lines before (restore) and after (restoretext) the text-format round trip
sed '/^restoretext /,$d' "$T/out" > "$T/first"
sed -n '/^restoretext /,$p' "$T/out" > "$T/second"
has() { grep -qxF -- "$2" "$T/$1"; }
# the checksums, computed here from the data the hive holds
ck() { python3 -c "import sys
d=bytes.fromhex(sys.argv[1]) if sys.argv[1] != 'big' else bytes((i*7)&0xff for i in range(40000))
s=0
for b in d: s=(s*31+b)&0xffffffff
print('%08x'%s)" "$1"; }
[ "$(v restore)" = 0 ] && pass "RegRestoreKeyW of a binary hive succeeds" || fail "restore: $(v restore)"
has first 'sz T/RootValue at the root' && has first "value T/Components/Count 4 4 $(ck 2a000000)" \
    && pass "the root key's values, and a subkey's inline DWORD (42)" || fail "root values"
has first 'sz T/Components/Path C:\Program Files\Test\core.dll' && has first 'sz T/Components/A8EF02CD/@ default value' \
    && pass "values in cells, the default value, subkeys from an li list" || fail "component values"
has first 'key T/Components/Deep/Deeper' && has first "value T/Components/Deep/Deeper/Leaf 3 6 $(ck 010203040506)" \
    && pass "nested keys and binary data" || fail "nested"
has first "value T/Big/Blob 3 40000 $(ck big)" && grep -q '^value T/Big/Multi 7 18 ' "$T/first" \
    && pass "big data (db, 40000 bytes, checksum right) and a multi-string" || fail "big data / multi: $(grep 'T/Big' "$T/first")"
has first 'key T/Ünïcode Kéy' && has first "value T/Ünïcode Kéy/Small 3 2 $(ck abcd)" && has first 'key T/Ri4' && has first 'key T/Ri1' \
    && pass "a UTF-16 key name, and subkeys from an ri list of lists" || fail "names / ri"
! grep -q 'Stale' "$T/first" && pass "what the key held before is gone (value and subkey)" || fail "stale contents remain"
[ "$(v save)" = 0 ] && [ "$(v restoretext)" = 0 ] && has second 'sz T/Components/Path C:\Program Files\Test\core.dll' \
    && has second 'key T/Components/Deep/Deeper' && has second "value T/Big/Blob 3 40000 $(ck big)" && ! grep -q Stale "$T/second" \
    && pass "a file RegSaveKey wrote restores too, replacing what was there" || fail "text round trip: $(v save) $(v restoretext)"
[ "$(v notahive)" != 0 ] && pass "a file that is not a hive is refused ($(v notahive))" || fail "not a hive accepted"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
