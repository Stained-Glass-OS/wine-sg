#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Small stubs that answered wrongly, second set (patches/sg/1642), 64- and
# 32-bit: test/smallstubs2-probe.c asks for the Linux side of a file
# (FileStatLxInformation), a directory's case sensitivity and a storage
# reserve id (STATUS_NOT_IMPLEMENTED before), expands a \\?\ path's short
# names (it came back unexpanded), reads the workgroup (a stand-in smb.conf
# for the 64-bit run: "Workgroup" before) and waits for a certificate store's
# change notification (never signalled before).
#
#   WINE=/opt/wine-sg/bin/wine test/smallstubs2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_LX_INFO (ntdll/unix/file.c), SG_MUTANT_UNC_AS_IS
# (kernelbase/file.c), SG_MUTANT_FIXED_WORKGROUP (netapi32/netapi32.c),
# SG_MUTANT_NO_STORE_NOTIFY (crypt32/regstore.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-smallstubs2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/smallstubs2-probe.c" -lcrypt32 -lnetapi32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/smallstubs2-probe.c" -lcrypt32 -lnetapi32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
printf '[global]\n   # a comment\n   workgroup = SGTESTGROUP \n[share]\n   workgroup = NOT\n' > "$T/smb.conf"
for p in p64 p32; do
    echo "== $p"
    if [ $p = p64 ]; then conf="$T/smb.conf" want=SGTESTGROUP; else conf="$T/none.conf" want=WORKGROUP; fi
    out=$(cd "$T" && SG_SMB_CONF="$conf" timeout -s KILL 120 "$WINE" "$T/$p.exe" $want 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
