#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# netapi32 local accounts and groups (patch 3180), 64- and 32-bit: test/fastnetacct-probe.c
# against a stand-in sg-admind spool (a shell loop that logs each request and
# answers it) and a stand-in /etc (SG_ACCOUNTS_ROOT) -- no real account is ever
# touched. An administrator's calls become sg-admind requests (user-add,
# user-remove, user-password, user-type) or registry changes (other groups);
# a non-administrator is refused before anything is filed.
#
#   WINE=/opt/wine-sg/bin/wine test/fastnetacct-gate.sh
# Mutants: SG_MUTANT_ACCT_NOADMIN (netapi32/localacct.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-fastnetacct.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DAEMON=
cleanup() { [ -n "$DAEMON" ] && kill "$DAEMON" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastnetacct-probe.c" -lnetapi32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$T/etc" "$T/spool/requests" "$T/spool/replies"
cat > "$T/etc/passwd" <<'EOS'
root:x:0:0:root:/root:/bin/bash
alice:x:1001:1001::/home/alice:/bin/bash
bob:x:1002:1002::/home/bob:/bin/bash
carol:x:1003:1003::/home/carol:/bin/bash
EOS
cat > "$T/etc/group" <<'EOS'
sgwine:x:2000:alice,bob,carol
sg-admins:x:2001:alice
EOS
export SG_ACCOUNTS_ROOT="$T/etc" SG_ADMIN_SPOOL="$T/spool"
: > "$T/log"; : > "$T/mode"
(
    while :; do
        for f in "$T"/spool/requests/*.req; do
            [ -e "$f" ] || continue
            id=$(basename "$f" .req)
            tr '\n' '|' < "$f" >> "$T/log"; echo >> "$T/log"
            rm -f "$f"
            if [ -s "$T/mode" ]; then echo "FAILED $(cat "$T/mode")" > "$T/spool/replies/$id.rep"
            else echo OK > "$T/spool/replies/$id.rep"; fi
        done
        sleep 0.1
    done
) &
DAEMON=$!
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

ADMIN_LOG='user-add|dave||standard|Secret1!|
user-remove|bob|keep|
user-password|carol|NewPass9!|
user-type|bob|administrator|
user-type|alice|standard|'
run() {
    name=$1; mode=$2; wantlog=$3; shift 3
    for a in x86_64 i686; do
        printf '%s' "$mode" > "$T/mode"; : > "$T/log"
        out=$(cd "$T" && timeout -s KILL 240 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
        sleep 0.3
        got=$(cat "$T/log")
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && [ "$got" = "$wantlog" ]; then
            echo "PASS  $name ($a)"
        else
            echo "FAIL  $name ($a)"
            printf '%s\n' "$out" | grep -v '^PASS' | sed 's/^/        /'
            [ "$got" != "$wantlog" ] && { echo "        log:"; printf '%s\n' "$got" | sed 's/^/        /'; }
            RC=1
        fi
        "$WINESERVER" -w
    done
}
run "an administrator's calls become sg-admind requests and registry changes" "" "$ADMIN_LOG" admin
run "a non-administrator is refused before anything is filed" "" "" nonadmin
run "sg-admind: account exists" "There is already an account named dave." "user-add|dave||standard|Secret1!|" reply user 2224
run "sg-admind: empty password" "The password cannot be empty." "user-add|dave||standard|Secret1!|" reply user 2245
run "sg-admind: invalid account name" "'dave' is not a valid account name: use lower-case." "user-add|dave||standard|Secret1!|" reply user 2202
run "sg-admind: the last administrator" "alice is the only administrator. Make another account an administrator first." "user-type|alice|standard|" reply admin 1322
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
