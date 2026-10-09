#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetStorageDependencyInformation on a file or volume that is not on a
# virtual disk (patches/sg/1528). Windows answers ERROR_VIRTDISK_NOT_VIRTUAL_DISK.
# Wine said success with no entries, and Microsoft 365 then read a host
# volume name from the empty buffer and faulted opening any existing
# workbook (Excel, from Recent or the command line).
#
#   WINE=/opt/wine-sg/bin/wine test/virtdiskdep-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-virtdiskdep.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/virtdiskdep-probe.c" -lvirtdisk || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/virtdiskdep-probe.c" -lvirtdisk || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    case "$O" in "file=c03a0015 vol=c03a0015 "*) pass "$a-bit: a file and a volume are not on a virtual disk" ;; *) fail "$a-bit not-virtual: $O" ;; esac
    case "$O" in *" zero=57 null=57") pass "$a-bit: no buffer is still a parameter error" ;; *) fail "$a-bit params: $O" ;; esac
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
