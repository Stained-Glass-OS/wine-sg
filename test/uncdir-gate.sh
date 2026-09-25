#!/bin/sh
# Gate for wine-sg 0206: cmd's `dir \\server\share` lists the share.
#
# cmd took an argument starting with "\" as relative to the current drive,
# so `dir \\dc1\shared` listed Z:\dc1 -- "Directory of Z:\dc1", "File not
# found". A share is made here the way sg-netmountd makes one (a mount point
# under /run/stained-glass-net/unc/<server>/<share>, patch 0056; a tmpfs, with
# sudo, removed afterwards).
#   WINE=... test/uncdir-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
UNC=/run/stained-glass-net/unc/sgtestsrv/share
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
mountpoint -q "$UNC" && { echo "SKIP: $UNC is in use"; exit 77; }

T=$(mktemp -d /var/tmp/sg-uncdir.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
cleanup() {
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -z "${WINESERVER:-}" ] || "$WINESERVER" -k 2>/dev/null
    sudo -n umount "$UNC" 2>/dev/null
    sudo -n rmdir "$UNC" /run/stained-glass-net/unc/sgtestsrv 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
sudo -n mkdir -p "$UNC" && sudo -n mount -t tmpfs -o size=1m,mode=0777 tmpfs "$UNC" || { echo "SKIP: cannot mount"; exit 77; }
echo marker > "$UNC/marker.txt"
mkdir "$UNC/sub" && echo inner > "$UNC/sub/inner.txt"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1

run() { (cd "$T" && timeout -s KILL 60 "$WINE" cmd /c "$1" 2>/dev/null | tr -d '\r'); }
out=$(run 'dir \\sgtestsrv\share')
printf '%s\n' "$out" | sed 's/^/  /'
case "$out" in *'Directory of \\sgtestsrv\share'*) pass "dir \\\\server\\share: Directory of \\\\sgtestsrv\\share" ;;
    *) fail "the listing's heading: $(printf '%s' "$out" | grep -i directory)" ;; esac
printf '%s\n' "$out" | grep -q ' marker.txt$' && pass "and lists the share's files" || fail "marker.txt is not listed"
printf '%s\n' "$out" | grep -q 'Z:' && fail "a Z: path appears" || pass "and no Z: path"
out=$(run 'dir \\sgtestsrv\share\sub')
case "$out" in *'Directory of \\sgtestsrv\share\sub'*) pass "a folder in the share" ;; *) fail "sub: $out" ;; esac
printf '%s\n' "$out" | grep -q ' inner.txt$' && pass "with its files" || fail "inner.txt is not listed"
out=$(run 'dir /b \\sgtestsrv\share\*.txt')
[ "$out" = marker.txt ] && pass "dir /b with a wildcard" || fail "dir /b: $out"

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
