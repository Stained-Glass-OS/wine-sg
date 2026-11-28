#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder shared on the network (patches/sg/9636, i0978): NetShareAdd and NetShareDel ask
# sg-admind (sg-shell; a stand-in here, test/sharing-server.py, on a spool of the gate's own) to
# make or remove a Samba share -- the name, the folder (as a Unix path), the comment, the user
# limit and Everyone's permission from the share's descriptor (read, change, full); its refusals
# become Windows' errors (the share name in use: NERR_DuplicateShare; file sharing off:
# NERR_ServerNotStarted; no such share: NERR_NetNameNotFound). NetShareEnum and NetShareGetInfo
# read the shares file back, with Windows paths. They were stubs: nothing could be shared, and
# File Explorer had no Sharing page (its page and Advanced Sharing are seen in the QA VM). 64-bit.
#   WINE=/opt/wine-sg/bin/wine test/sharing-gate.sh
# Mutants: SG_MUTANT_NO_LOCAL_SHARES (dlls/netapi32/netapi32.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WINESERVER=$(sg_wineserver); export WINESERVER
T=$(mktemp -d /var/tmp/sg-sharing.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
SPID=
cleanup() { [ -n "$SPID" ] && kill "$SPID" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/sharing-probe.c" -lnetapi32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$T/spool/requests" "$T/spool/replies" "$T/work"
export SG_ADMIN_SPOOL="$T/spool" SG_SHARES_CONF="$T/shares.conf"
python3 "$HERE/sharing-server.py" "$T/spool" "$T/shares.conf" "$T/requests" &
SPID=$!
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 60 "$WINESERVER" -w
RC=0
ok() { printf "PASS  %s\n" "$*"; }
bad() { printf "FAIL  %s\n" "$*"; RC=1; }
out=$(cd "$T/work" && timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p" | head -1; }
first=$(head -1 "$T/requests" 2>/dev/null)
[ "$(v ADD)" = 0 ] && [ "$first" = "share-add|Projects|$T/work/shared folder|Team files|3|change" ] \
    && ok "NetShareAdd asks sg-admind: name, the folder's Unix path, comment, limit, Everyone change" || bad "add: $(v ADD) / $first"
grep -qx "share-add|Docs|$T/work/shared folder||0|read" "$T/requests" && [ "$(v ADDREAD)" = 0 ] \
    && ok "...no limit is 0, Everyone read is read" || bad "add read: $(v ADDREAD) / $(cat "$T/requests")"
[ "$(v DUP)" = 2118 ] && ok "a share name in use is NERR_DuplicateShare" || bad "dup: $(v DUP)"
[ "$(v OFF)" = 2114 ] && ok "file sharing turned off is NERR_ServerNotStarted" || bad "off: $(v OFF)"
dos=$(printf '%s\n' "$out" | sed -n 's/^SHARE Projects|\([^|]*\)|.*/\1/p')
printf '%s\n' "$out" | grep -q '^SHARE Projects|.*\\shared folder|Team files|3|rw$' && printf '%s\n' "$out" | grep -q '^SHARE Docs|.*|-1|ro$' \
    && ok "NetShareEnum lists both, with a Windows path ($dos), comment, limit and access" || bad "enum: $out"
[ "$(v GETINFO)" = 0 ] && [ "$(v GOT)" = Projects ] && ok "NetShareGetInfo finds a share by name, in any case" || bad "getinfo: $(v GETINFO) $(v GOT)"
[ "$(v DEL)" = 0 ] && grep -qx 'share-remove|Projects' "$T/requests" && [ "$(v DEL2)" = 2310 ] \
    && ok "NetShareDel asks sg-admind; no such share is NERR_NetNameNotFound" || bad "del: $(v DEL) $(v DEL2)"
[ "$(v RUNDLL)" = 1 ] && ok "shell32 has the Sharing page's elevated half (SgShareFolderW)" || bad "no SgShareFolderW"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
