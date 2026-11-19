#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What System Informer's setup needs from ntdll (patches/sg/7900, 7901). Its setup ended at once with
# "unimplemented function ntdll.dll.RtlGetFullPathName_UEx", then with NtOpenPrivateNamespace.
#  - RtlGetFullPathName_UEx: RtlGetFullPathName_U's byte count through an out parameter, with a
#    status (the bytes written, or the bytes needed with the NUL when the buffer is too small --
#    still STATUS_SUCCESS; an empty name is STATUS_OBJECT_NAME_INVALID).
#  - NtCreatePrivateNamespace / NtOpenPrivateNamespace / NtDeletePrivateNamespace: no name, the
#    boundary descriptor is the namespace (System Informer's PhCreatePrivateNamespace); objects go
#    in by RootDirectory = the namespace handle; another process with an equal descriptor reaches
#    the same namespace; a boundary SID the caller does not hold is STATUS_ACCESS_DENIED; a second
#    create is a collision; the namespace goes with its last handle.
# test/sysinformer-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/sysinformer-gate.sh
# Mutants (dlls/ntdll): -DSG_MUTANT_FULLPATH_UEX_NO_BYTES (no byte count),
# -DSG_MUTANT_PRIVNS_NO_BOUNDARY_CHECK (anyone may create a namespace whatever its SIDs).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sysinformer.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/sysinformer-probe.c" -lntdll || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check exports      "ntdll exports RtlGetFullPathName_UEx and Nt{Create,Open,Delete}PrivateNamespace"
    check fullpath     "RtlGetFullPathName_UEx: full path, file part, bytes written without the NUL"
    check shortbuf     "a short buffer is STATUS_SUCCESS with the bytes needed, NUL included"
    check invalid      "an empty name is STATUS_OBJECT_NAME_INVALID"
    check absent       "opening a namespace no one made is STATUS_OBJECT_NAME_NOT_FOUND"
    check create       "NtCreatePrivateNamespace with a boundary descriptor and no name"
    check collision    "a second create of the same boundary is STATUS_OBJECT_NAME_COLLISION"
    check sameobject   "a mutex made in it is found through a second NtOpenPrivateNamespace"
    check otherprocess "another process with an equal descriptor reaches the same namespace"
    check denied       "a boundary SID the caller does not hold is STATUS_ACCESS_DENIED"
    check byname       "a descriptor with another name is another namespace"
    check delete       "NtDeletePrivateNamespace takes the namespace handle, refuses a bad one"
    check gone         "the namespace goes with its last handle"
    check done         "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
