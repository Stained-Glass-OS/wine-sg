#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A share connected with a name and password (patches/sg/0474).
#
# A Windows PC's shares refuse a guest: File Explorer listed them with no
# files in them. Now the connection is made with a name and password for the
# user alone -- sg-netmountd's LOGON mounts it under
# /run/stained-glass-net/users/<uid>/unc/<server>/<share> -- and:
# - ntdll looks there before the share every user has;
# - net use \\server\share PASSWORD /USER:DOMAIN\name, and
#   WNetAddConnection2 with a name, ask sg-netmountd for LOGON (a stand-in
#   listens on its socket here and records what it is asked), then MOUNT;
#   net use \\server\share /DELETE asks for LOGOFF;
# - File Explorer asks "Enter network credentials" (credui) when a share
#   will not open (seen in the QA VM; not here).
# Mounts are tmpfs, made and removed with sudo.
#   WINE=... test/netlogon-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
NET=/run/stained-glass-net
ME=$(id -u)
USERS_SHARE=$NET/users/$ME/unc/sgtestsrv/share
SHARED=$NET/unc/sgtestsrv/share
SOCK=$NET/netmount.sock
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: no python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -e "$SOCK" ] && { echo "SKIP: $SOCK is in use (a real sg-netmountd)"; exit 77; }
mountpoint -q "$SHARED" || mountpoint -q "$USERS_SHARE" && { echo "SKIP: test mounts in use"; exit 77; }

T=$(mktemp -d /var/tmp/sg-netlogon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
LP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$LP" ] && sudo -n kill "$LP" 2>/dev/null
    sudo -n rm -f "$SOCK"
    sudo -n umount "$USERS_SHARE" "$SHARED" 2>/dev/null
    sudo -n rmdir "$USERS_SHARE" "$NET/users/$ME/unc/sgtestsrv" "$NET/users/$ME/unc" "$NET/users/$ME" "$NET/users" \
        "$SHARED" "$NET/unc/sgtestsrv" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

sudo -n mkdir -p "$USERS_SHARE" "$SHARED"
sudo -n mount -t tmpfs -o size=1m,mode=0755 sgtest "$SHARED"
sudo -n mount -t tmpfs -o "size=1m,mode=0700,uid=$ME" sgtest "$USERS_SHARE"
sudo -n sh -c "echo shared > '$SHARED/everyones.txt'"
echo mine > "$USERS_SHARE/mine.txt"

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
out=$("$WINE" cmd /c 'dir /b \\sgtestsrv\share' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
[ "$out" = mine.txt ] && pass "\\\\server\\share is the user's own connection when there is one" \
                      || fail "\\\\server\\share lists: $out"

# a stand-in sg-netmountd: records each request line, answers OK
sudo -n umount "$USERS_SHARE"
cat > "$T/listen.py" <<'PY'
import os, socket, sys
path, log = sys.argv[1], sys.argv[2]
s = socket.socket(socket.AF_UNIX)
s.bind(path); os.chmod(path, 0o666); s.listen(8)
while True:
    c, _ = s.accept()
    f = c.makefile("rb")
    for line in f:
        with open(log, "ab") as l: l.write(line)
        c.sendall(b"OK /nowhere\n")
    c.close()
PY
touch "$T/requests"; chmod 666 "$T/requests"
sudo -n python3 "$T/listen.py" "$SOCK" "$T/requests" & LP=$!
i=0; while [ ! -S "$SOCK" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
"$WINE" net use '\\sgtestsrv\share' 'Pw 1,2=3!' '/USER:SGDOM\bob' >/dev/null 2>&1
"$WINE" net use '\\sgtestsrv\share' /delete >/dev/null 2>&1
sleep 1
sed 's/^/      /' "$T/requests"
grep -qxF "$(printf 'LOGON\tsgtestsrv\tshare\tSGDOM\\bob\tPw 1,2=3!')" "$T/requests" \
    && pass "net use with /USER: and a password asks sg-netmountd to LOGON with them" || fail "no LOGON request"
grep -qxF "$(printf 'MOUNT\tsgtestsrv\tshare')" "$T/requests" && pass "and then to MOUNT" || fail "no MOUNT after LOGON"
grep -qxF "$(printf 'LOGOFF\tsgtestsrv\tshare')" "$T/requests" && pass "net use /DELETE asks to LOGOFF" || fail "no LOGOFF"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
