#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32: the context menu of a file whose extension has no class takes no verbs from
# HKEY_CLASSES_ROOT's own keys (6031). The type's name was left unset; an empty or stray name opened
# HKEY_CLASSES_ROOT itself, and its "shell" key (the shell: protocol) gave the menu a "shell" item.
#   WINE=/opt/wine-sg/bin/wine test/unknowntype-gate.sh
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
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-unknowntype.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/pkgverb-probe.exe" "$HERE/pkgverb-probe.c" -lole32 -lshell32 -luuid || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/pkgverb-probe.exe" "$C/"
mkdir -p "$C/Data"
echo x > "$C/Data/a.zzq"
echo x > "$C/Data/b.txt"
run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }

out=$(run reg query 'HKCR\shell')
printf '%s\n' "$out" | grep -q '^HKEY' && pass "HKEY_CLASSES_ROOT has a shell key (the shell: protocol)" ||
    fail "no HKCR\\shell: the check below proves nothing"
for i in 1 2 3; do
    out=$(run pkgverb-probe.exe menu 'C:\Data\a.zzq')
    if printf '%s\n' "$out" | grep -qi '^item=shell|'; then fail "a file of an unknown type has a 'shell' item ($i)"; break; fi
done
printf '%s\n' "$out" | grep -qi '^item=shell|' || pass "a file of an unknown type has no 'shell' item"
printf '%s\n' "$out" | grep -q '^item=Open wit&h' && pass "it has Open with" || fail "no Open with: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run pkgverb-probe.exe menu 'C:\Data\b.txt')
printf '%s\n' "$out" | grep -q '^item=&Open|' && pass "a known type keeps its verbs" || fail "a .txt file's verbs: $(printf '%s' "$out" | tr '\n' ' ')"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
