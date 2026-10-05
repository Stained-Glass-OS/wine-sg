#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The connections a user has made (patches/sg/0826, 0838): NET USE lists
# them -- it printed nothing, not even "There are no entries in the list."
# (NetUseEnum was a stub; mutant SG_MUTANT_NETUSEENUM_STUB in
# dlls/netapi32/netapi32.c) -- including a drive mapped with a name and
# password (net use /user:), which the network provider called "not
# connected" (mutant SG_MUTANT_USER_DRIVE_UNKNOWN in
# dlls/ntlanman/ntlanman.c); and File Explorer names a network drive for its
# folder, "big (\\server) (S:)", not "Network Drive (S:)" (mutant
# SG_MUTANT_NETDRIVE_GENERIC_NAME in dlls/shell32/shfldr_mycomp.c).
#
# The drives are made as sg-netmountd makes them (with sudo, removed after):
# tmpfs shares under /run/stained-glass-net/unc/<server>/<share> and
# .../users/<uid>/unc/<server>/<share>, the user's letters as links in
# .../drives/<uid>/; test/smbshim.c makes the shares look like SMB mounts.
#
#   WINE=... test/netuse-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
NET=/run/stained-glass-net
SHARE=$NET/unc/sgnetsrv/big
OWN=$NET/users/$(id -u)/unc/sgnetsrv/private
DRIVES=$NET/drives/$(id -u)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
for d in "$SHARE" "$OWN" "$DRIVES"; do [ -e "$d" ] && { echo "SKIP: $d is in use"; exit 77; }; done
made_net=; [ -d "$NET" ] || made_net=1

T=$(mktemp -d /var/tmp/sg-netuse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    sudo -n umount "$SHARE" "$OWN" 2>/dev/null
    sudo -n rm -rf "$DRIVES" "$NET/unc/sgnetsrv" "$NET/users/$(id -u)"
    [ -n "$made_net" ] && sudo -n rm -rf "$NET" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
gcc -shared -fPIC -O2 -o "$T/smbshim.so" "$HERE/smbshim.c" -ldl || { echo "FAIL  the shim did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/netuse-probe.exe" "$HERE/netuse-probe.c" -lnetapi32 -lshell32 || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/netuse-probe.exe" "$WINEPREFIX/drive_c/"
run() {
    (cd "$WINEPREFIX/drive_c" && LD_PRELOAD="$T/smbshim.so" SG_SMBSHIM_ROOT="$NET" \
        timeout 120 "$WINE" "$@" > "$T/out" 2>/dev/null </dev/null)
    tr -d '\r' < "$T/out"
}

out=$(run net use)
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in *'There are no entries in the list.'*) pass "NET USE with nothing connected says so" ;; *) fail "NET USE with nothing connected: '$out'" ;; esac

sudo -n mkdir -p "$SHARE" "$OWN" "$DRIVES" &&
    sudo -n mount -t tmpfs -o size=1m,mode=0777 tmpfs "$SHARE" && sudo -n mount -t tmpfs -o size=1m,mode=0700,uid="$(id -u)" tmpfs "$OWN" &&
    sudo -n chown "$(id -u)" "$DRIVES" || { echo "SKIP: cannot mount"; exit 77; }
echo hello > "$SHARE/hello.txt"; echo secret > "$OWN/secret.txt"
ln -s "$SHARE" "$DRIVES/s:"; ln -s "$OWN" "$DRIVES/p:"
sleep 1.5   # ntdll reads a user's letters again at most once a second

out=$(run net use)
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -Eq '^OK +S: +\\\\sgnetsrv\\big$' && pass "NET USE lists S: \\\\sgnetsrv\\big" || fail "S: is not listed"
printf '%s\n' "$out" | grep -Eq '^OK +P: +\\\\sgnetsrv\\private$' && pass "and P:, connected with a name and password, as \\\\sgnetsrv\\private" ||
    fail "P: (a connection with a name and password) is not listed"
case "$out" in *'The command completed successfully.'*) pass "and ends as Windows' does" ;; *) fail "no closing line" ;; esac

out=$(run 'C:\netuse-probe.exe' 'S:\' 'P:\')
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in *'netuseenum=0 count=2'*) pass "NetUseEnum gives programs both connections" ;; *) fail "NetUseEnum: $(printf '%s\n' "$out" | head -1)" ;; esac
case "$out" in *'name[S:\]=big (\\sgnetsrv) (S:) type=4'*) pass "File Explorer names S: 'big (\\\\sgnetsrv) (S:)'" ;;
    *) fail "S:'s name: $(printf '%s\n' "$out" | grep 'name\[S')" ;; esac
case "$out" in *'name[P:\]=private (\\sgnetsrv) (P:) type=4'*) pass "and P: 'private (\\\\sgnetsrv) (P:)'" ;;
    *) fail "P:'s name: $(printf '%s\n' "$out" | grep 'name\[P')" ;; esac
out=$(run cmd /c 'type P:\secret.txt & dir S:\')
case "$out" in *secret*'Volume in drive S has no label.'*'hello.txt'*) pass "and programs use both (cmd: TYPE on P:, DIR of S: with its volume)" ;;
    *) fail "cmd on the drives: $(printf '%s ' "$out" | head -c 300)" ;; esac

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
