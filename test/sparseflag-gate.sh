#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A file's sparse flag and fsutil sparse (patches/sg/9810). On Windows FSCTL_SET_SPARSE marks a file
# sparse: its attributes then carry FILE_ATTRIBUTE_SPARSE_FILE, setting attributes neither sets nor
# clears it, FSCTL_SET_SPARSE(FALSE) clears it, and fsutil sparse queryflag/setflag/queryrange/
# setrange read and change it. Wine ignored the request and fsutil had no "sparse" command (Electrum:
# "error marking file ...blockchain_headers as sparse: ... fsutil sparse setflag ... returned
# non-zero exit status 1"). The probe (test/sparseflag-probe.c) checks the flag through the API; the
# gate then drives fsutil itself.
#   WINE=/opt/wine-sg/bin/wine test/sparseflag-gate.sh
# Mutants: SG_MUTANT_NO_SPARSE_FLAG (ntdll/unix/file.c), SG_MUTANT_NO_FSUTIL_SPARSE (fsutil/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sparseflag.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/sparseflag-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
l() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
[ "$(l INIT)" = 0 ] && pass "a new file is not sparse" || fail "new file: '$(l INIT)'"
[ "$(l SETCALL)" = 0 ] && [ "$(l SET)" = 200 ] && pass "FSCTL_SET_SPARSE: the file reports FILE_ATTRIBUTE_SPARSE_FILE" || fail "set: call '$(l SETCALL)' attributes '$(l SET)' (want 200)"
[ "$(l KEEP)" = 200 ] && [ "$(l HIDDEN)" = 202 ] && pass "SetFileAttributes keeps the sparse flag (NORMAL, then HIDDEN)" || fail "kept: '$(l KEEP)' '$(l HIDDEN)' (want 200 202)"
[ "$(l FIND)" = 202 ] && pass "FindFirstFile reports it too" || fail "find: '$(l FIND)' (want 202)"
[ "$(l READONLY)" = 5 ] && [ "$(l STILL)" = 202 ] && pass "a handle without write access may not change it (ERROR_ACCESS_DENIED)" || fail "read-only handle: '$(l READONLY)' '$(l STILL)' (want 5 202)"
[ "$(l CLEARCALL)" = 0 ] && [ "$(l CLEAR)" = 2 ] && pass "FSCTL_SET_SPARSE(FALSE) clears it, the other attributes stay" || fail "clear: '$(l CLEARCALL)' '$(l CLEAR)' (want 0 2)"
[ "$(l CREATE)" = 0 ] && pass "CreateFile with FILE_ATTRIBUTE_SPARSE_FILE does not make a sparse file" || fail "create: '$(l CREATE)' (want 0)"

C="$WINEPREFIX/drive_c"
head -c 262144 /dev/zero | tr '\0' 'y' > "$C/sp/e.bin"
fs() { timeout 60 "$WINE" fsutil "$@" >"$T/fs.out" 2>/dev/null; r=$?; tr -d '\r' < "$T/fs.out"; return $r; }
fs sparse setflag 'C:\sp\e.bin' >/dev/null; r1=$?
q1=$(fs sparse queryflag 'C:\sp\e.bin')
[ $r1 = 0 ] && [ "$q1" = "This file is set as sparse" ] && pass "fsutil sparse setflag, queryflag: set as sparse" || fail "fsutil setflag rc $r1, queryflag '$q1'"
fs sparse setrange 'C:\sp\e.bin' 65536 65536 >/dev/null; r2=$?
ranges=$(fs sparse queryrange 'C:\sp\e.bin')
printf '      %s\n' "$ranges"
want="sparse range: [Offset: 0x0] [Length: 0x10000]
sparse range: [Offset: 0x20000] [Length: 0x20000]"
[ $r2 = 0 ] && [ "$ranges" = "$want" ] && pass "fsutil sparse setrange makes a hole, queryrange lists the data around it" || fail "setrange rc $r2, ranges as above"
fs sparse setflag 'C:\sp\e.bin' 0 >/dev/null; r3=$?
q2=$(fs sparse queryflag 'C:\sp\e.bin')
[ $r3 = 0 ] && [ "$q2" = "This file is NOT set as sparse" ] && pass "fsutil sparse setflag 0 clears it" || fail "fsutil setflag 0 rc $r3, queryflag '$q2'"
sz=$(stat -c %s "$C/sp/e.bin")
[ "$sz" = 262144 ] && pass "the file keeps its size" || fail "size $sz"
fs sparse setflag 'C:\sp\missing.bin' >/dev/null; [ $? = 1 ] && pass "a missing file: exit code 1" || fail "missing file did not fail"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
