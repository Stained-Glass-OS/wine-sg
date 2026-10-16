#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# authz.dll (patches/sg/5513): resource manager, client contexts from a
# token, AuthzAccessCheck (the context's token decides, optional descriptors
# are concatenated, NULL DACL, principal-self SID, argument checks). 64- and
# 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/authz-gate.sh
# Mutants (authz.c; build with x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_AZ_PROCESS_TOKEN (checks the process token, not the context's),
# SG_MUTANT_AZ_NO_CONCAT (ignores the optional descriptors),
# SG_MUTANT_AZ_NO_PRIVILEGE (no SeAuditPrivilege check),
# SG_MUTANT_AZ_ALWAYS_GRANT (grants what is asked).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-authz.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
x86_64-w64-mingw32-gcc -O2 -o "$T/az64.exe" "$HERE/authz-probe.c" -lauthz -ladvapi32 || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/az32.exe" "$HERE/authz-probe.c" -lauthz -ladvapi32 || { echo "FAIL  probe"; exit 1; }
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

for b in 64 32; do
    out=$(DISPLAY= timeout -s KILL 120 "$WINE" "$T/az$b.exe" 2>/dev/null | tr -d '\r')
    n_ok=$(printf '%s\n' "$out" | grep -c '^ok ')
    printf '%s\n' "$out" | grep '^FAIL' | while read -r line; do printf 'FAIL  %s-bit %s\n' "$b" "$line"; done
    if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -qx 'authz-probe: all ok'; then
        RC=1
        printf 'FAIL  %s-bit probe (%s checks ok)\n' "$b" "$n_ok"
    else
        printf 'PASS  %s-bit %s checks\n' "$b" "$n_ok"
    fi
    # the restricted client must really differ from the process for the context check to mean something
    if printf '%s\n' "$out" | grep -q '^ok context-decides-not-process'; then
        printf 'PASS  %s-bit the context, not the process, decides\n' "$b"
    else
        RC=1; printf 'FAIL  %s-bit context-decides-not-process did not run or failed (%s)\n' "$b" "$(printf '%s\n' "$out" | grep -E 'info|restricted' | head -2)"
    fi
done

[ $RC = 0 ] && echo "authz-gate: all passed" || echo "authz-gate: FAILURES"
exit $RC
