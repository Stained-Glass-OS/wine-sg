#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Credential Manager's names and passwords for servers go to the logon session (sg-netmountd
# CRED / UNCRED; wine-sg 9631, i0976), so that any program opening \\server\share is
# connected with them, as Windows' redirector does -- not File Explorer and net use alone.
# CredWrite of a domain credential for a computer hands it over (with its secret when the
# write preserves it), CredDelete forgets it, CredProfileLoaded (sign-in) hands over all of
# them; generic credentials and targets that are no computer (TERMSRV/..., *.domain) never
# go. A stand-in daemon (test/netcred-server.py) records the requests. With no daemon a
# write is not held up. 64- and 32-bit. No keyring: the registry keeps the credentials.
#
#   WINE=/opt/wine-sg/bin/wine test/netcred-gate.sh
# Mutants: SG_MUTANT_NO_NET_CREDENTIAL (dlls/advapi32/cred.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
WINESERVER=$(sg_wineserver); export WINESERVER
T=$(mktemp -d /var/tmp/sg-netcred.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
SPID=
cleanup() { [ -n "$SPID" ] && kill "$SPID" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/netcred-probe.c" -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
export SG_NETMOUNT_SOCK="$T/netmount.sock"
python3 "$HERE/netcred-server.py" "$SG_NETMOUNT_SOCK" "$T/requests" &
SPID=$!
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 60 "$WINESERVER" -w
RC=0
ok() { echo "      PASS  $*"; }
bad() { echo "      FAIL  $*"; RC=1; }
run() { (cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe-$1.exe" "$2" 2>/dev/null </dev/null | tr -d '\r' | sed 's/^/        /'); }
got() { tr '\t' '|' < "$T/requests" 2>/dev/null; }
for a in x86_64 i686; do
    echo "== $a"
    : > "$T/requests"
    run $a write
    r=$(got)
    want="CRED|FILESRV|FILESRV\\bob|Pw 1,2=3!
CRED|nas.example.lan|alice|s3crét"
    [ "$r" = "$want" ] && ok "a domain credential for a computer is handed over, name and password exactly (UTF-8)" \
        || { bad "what was handed over:"; printf '%s\n' "$r" | sed 's/^/          /'; }
    case "$r" in *git:*|*TERMSRV*|*corp*|*token*) bad "a generic credential or a target that is no computer went";; *) ok "generic credentials, TERMSRV/ and *.domain targets stay";; esac
    : > "$T/requests"
    run $a preserve
    [ "$(got)" = 'CRED|FILESRV|FILESRV\robert|Pw 1,2=3!' ] && ok "a write that keeps the secret hands over the kept password with the new name" \
        || bad "preserve: $(got)"
    : > "$T/requests"
    run $a profile
    r=$(got | sort)
    [ "$(printf '%s\n' "$r" | grep -c '^CRED|')" = 2 ] && printf '%s\n' "$r" | grep -q '^CRED|nas.example.lan|alice|' \
        && printf '%s\n' "$r" | grep -q '^CRED|FILESRV|FILESRV\\robert|' \
        && ok "CredProfileLoaded (sign-in) hands over every one kept for a computer" || { bad "profile:"; printf '%s\n' "$r" | sed 's/^/          /'; }
    : > "$T/requests"
    run $a delete
    [ "$(got)" = 'UNCRED|FILESRV' ] && ok "CredDelete forgets it in the logon session" || bad "delete: $(got)"
    (cd "$T" && timeout -s KILL 60 "$WINE" cmd /c "reg delete \"HKCU\\Software\\Wine\\Credential Manager\" /f" >/dev/null 2>&1)
done
kill "$SPID" 2>/dev/null; wait "$SPID" 2>/dev/null; SPID=
rm -f "$SG_NETMOUNT_SOCK"
out=$(run x86_64 nodaemon)
printf '%s\n' "$out"
ms=$(printf '%s\n' "$out" | sed -n 's/.*took \([0-9]*\) ms/\1/p')
printf '%s\n' "$out" | grep -q 'write OTHER: 1' && [ "${ms:-99999}" -lt 3000 ] && ok "with no daemon a write still succeeds at once (${ms} ms)" \
    || bad "no daemon: $out"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
