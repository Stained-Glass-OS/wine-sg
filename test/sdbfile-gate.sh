#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# apphelp/sdbinst (patches/sg/6059): P-SDB, application compatibility databases.
#  - apphelp writes a database (SdbCreateDatabase through an NT path, list tags, a tag of every type,
#    STRINGREF strings in one string table item per distinct string, a STRING tag in place, an odd-sized
#    binary tag padded) and reads it back: SdbGetDatabaseVersion 2.1, each list's exact children,
#    SdbFindFirst/NextTag, the typed reads (a wrong type gives the default), SdbReadStringTag's buffer
#    size, SdbGetDatabaseID, SdbTagToString, SdbGUIDToString/FromString, SdbIsNullGUID, SdbInitDatabase;
#    a file that is not a database does not open;
#  - sdbinst installs it: the file in <windows>\AppPatch\Custom\{GUID}.sdb, AppCompatFlags\InstalledSDB\{GUID}
#    (DatabaseDescription, DatabasePath, DatabaseType, DatabaseInstallTimeStamp), AppCompatFlags\Custom\<exe>
#    for each EXE, an Uninstall entry; -u file.sdb removes all of it, as do -g {GUID} and -n name.
#   WINE=/opt/wine-sg/bin/wine test/sdbfile-gate.sh   (mutants SG_MUTANT_SDB_REF_FROM_TABLE,
#   SG_MUTANT_SDB_NO_DEDUP, SG_MUTANT_SDB_LIST_SIZE_HEADER (dlls/apphelp/sdb.c),
#   SG_MUTANT_SDBINST_NO_EXE (programs/sdbinst/main.c))
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sdbfile.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/sdbfile-probe.exe" "$HERE/sdbfile-probe.c" || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/sdbfile-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" sdbfile-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect create 1 "SdbCreateDatabase takes an NT path"
expect wrongtype 0 "a write of the wrong type for its tag fails"
expect version "1 2.1" "the database is version 2.1"
expect open 1 "SdbOpenDatabase opens it"
expect toplevel 2 "two top-level tags: DATABASE and the STRINGTABLE"
expect database "1 4" "DATABASE holds NAME, DATABASE_ID and two EXEs, nothing more"
expect exes "1 1 1 9" "SdbFindFirst/NextTag find both EXEs; the first holds its nine tags exactly"
expect exe2children 1 "the last list ends where it should"
expect name app.exe "a STRINGREF tag reads through the string table"
expect vendor "SG Test Fixes" "a repeated string reads back"
expect name2 other.exe "and the second EXE's"
expect readstring "0 1 other.exe" "SdbReadStringTag needs room for the terminator"
expect null 1 "a NULL tag has no data"
expect byte 0xab "BYTE"
expect word 0x1234 "WORD"
expect dword 0xdeadbeef "DWORD"
expect qword 0x123456789abcdef "QWORD"
expect default 0x7 "a read of the wrong type gives the default"
expect direct direct "a STRING tag is written in place"
expect binary "5 0 1 1" "an odd-sized BINARY tag, and SdbReadBinaryTag's buffer size"
expect stringtable "1 3" "one string table item per distinct string"
expect tagname "EXE DATABASE_ID InvalidTag" "SdbTagToString"
expect dbid "1 1" "SdbGetDatabaseID"
expect guidstring "{5A4E3D2C-1B0A-4F9E-8D7C-6B5A49382716}" "SdbGUIDToString"
expect guidparse "1 1 0 1" "SdbGUIDFromString, SdbIsNullGUID"
expect bad 1 "a file that is not a database does not open"
expect init "1 1 1" "SdbInitDatabase: a given database, no database, a bad one"
expect done 1 "the probe ran to the end"

# sdbinst
G='{5A4E3D2C-1B0A-4F9E-8D7C-6B5A49382716}'
K='HKLM\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags'
U='HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall'
reg() { timeout -s KILL 60 "$WINE" reg query "$@" 2>/dev/null </dev/null | tr -d '\r' | grep -v '^reg: '; }
installed_file() { ls "$C/windows/AppPatch/Custom/$G.sdb" >/dev/null 2>&1; }
check_installed() {   # check_installed HOW
    o=$(reg "$K\\InstalledSDB\\$G")
    if printf '%s\n' "$o" | grep -q 'DatabaseDescription.*REG_SZ.*SG Test Fixes' &&
       printf '%s\n' "$o" | grep -q 'DatabaseType.*REG_DWORD.*0x10000' &&
       printf '%s\n' "$o" | grep -qi "DatabasePath.*REG_SZ.*AppPatch.Custom.$G.sdb" &&
       printf '%s\n' "$o" | grep -q 'DatabaseInstallTimeStamp.*REG_QWORD'; then pass "$1: InstalledSDB\\{GUID} describes it"
    else fail "$1: InstalledSDB\\{GUID}: $(printf '%s' "$o" | tr '\n' '|')"; fi
    if installed_file; then pass "$1: the database is in AppPatch\\Custom"; else fail "$1: no AppPatch\\Custom\\$G.sdb"; fi
    a=$(reg "$K\\Custom\\app.exe"); b=$(reg "$K\\Custom\\other.exe")
    if printf '%s\n' "$a" | grep -qF "$G.sdb" && printf '%s\n' "$b" | grep -qF "$G.sdb"; then pass "$1: Custom\\<exe> lists it for both EXEs"
    else fail "$1: Custom\\app.exe: $(printf '%s' "$a" | tr '\n' '|') other.exe: $(printf '%s' "$b" | tr '\n' '|')"; fi
    if reg "$U\\$G.sdb" | grep -q 'UninstallString.*sdbinst'; then pass "$1: an Uninstall entry"; else fail "$1: no Uninstall entry"; fi
}
check_removed() {   # check_removed HOW
    if [ -z "$(reg "$K\\InstalledSDB\\$G")" ] && [ -z "$(reg "$K\\Custom\\app.exe")" ] &&
       [ -z "$(reg "$K\\Custom\\other.exe")" ] && [ -z "$(reg "$U\\$G.sdb")" ] && ! installed_file; then
        pass "$1 removes the file and every key"
    else fail "$1 left: $(reg "$K\\InstalledSDB\\$G" | head -2 | tr '\n' '|') $(reg "$K\\Custom\\app.exe" | head -2 | tr '\n' '|') $(installed_file && echo file)"; fi
}
sdbinst() { (cd "$C" && timeout -s KILL 60 "$WINE" sdbinst.exe "$@" 2>&1 </dev/null | tr -d '\r'); }
o=$(sdbinst -q 'C:\sg.sdb')
[ -z "$o" ] && pass "sdbinst -q is quiet" || fail "sdbinst -q printed: $o"
check_installed "sdbinst file.sdb"
sdbinst -q -u 'C:\sg.sdb' >/dev/null
check_removed "sdbinst -u file.sdb"
sdbinst -q 'C:\sg.sdb' >/dev/null
sdbinst -q -g "$G" >/dev/null
check_removed "sdbinst -g {GUID}"
o=$(sdbinst 'C:\sg.sdb')
printf '%s\n' "$o" | grep -q 'Installation of SG Test Fixes complete' && pass "sdbinst reports the install" || fail "sdbinst said: $o"
sdbinst -q -n 'SG Test Fixes' >/dev/null
check_removed "sdbinst -n name"
o=$(cd "$C" && timeout -s KILL 60 "$WINE" sdbinst.exe -q 'C:\bad.sdb' >/dev/null 2>&1 </dev/null; echo $?); [ "$o" = 1 ] && pass "a file that is not a database: exit 1" || fail "bad.sdb: exit $o"
o=$(cd "$C" && timeout -s KILL 60 "$WINE" sdbinst.exe -q -g "$G" >/dev/null 2>&1 </dev/null; echo $?); [ "$o" = 1 ] && pass "uninstalling what is not installed: exit 1" || fail "not installed: exit $o"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
