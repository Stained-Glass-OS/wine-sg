#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# MSDASQL's IDBSchemaRowset: DBSCHEMA_FOREIGN_KEYS, DBSCHEMA_INDEXES (statistics
# rows left out) and DBSCHEMA_PROVIDER_TYPES through ODBC's SQLForeignKeys,
# SQLStatistics and SQLGetTypeInfo (patches/sg/8224), 64-bit, over
# test/adomock-driver.c: test/adoschema-probe.c. They were E_INVALIDARG.
# (8230) DBSCHEMA_VIEWS and DBSCHEMA_PROCEDURE_PARAMETERS through SQLTables
# (table type VIEW) and SQLProcedureColumns (result-set columns left out).
#
#   WINE=/opt/wine-sg/bin/wine test/adoschema-gate.sh
# Mutants: SG_MUTANT_NO_MORE_SCHEMAS, SG_MUTANT_NO_VIEW_PARAM_SCHEMAS (msdasql/session.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-adoschema.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for b in 64; do
    cc=x86_64-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O2 -shared -o "$T/adomock$b.dll" "$HERE/adomock-driver.c" &&
    TMPDIR=/var/tmp $cc -O2 -o "$T/p$b.exe" "$HERE/adoschema-probe.c" -lole32 -loleaut32 -luuid ||
        { fail "the probe or driver did not build ($b)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T"/adomock64.dll "$T"/p64.exe "$C/"
"$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock64.dll' /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBob\t40\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'
for b in p64; do
    out=$(cd "$C" && timeout -s KILL 120 "$WINE" "C:\\$b.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -q '^DONE' || { fail "$b: the probe did not finish"; continue; }
    for kv in schemas_listed=111 foreign_keys=00000000 fk_rows=1 fk0.PK_TABLE_NAME=People fk0.PK_COLUMN_NAME=Id \
              fk0.FK_TABLE_NAME=Pets fk0.FK_COLUMN_NAME=OwnerId fk0.ORDINAL=1 fk0.UPDATE_RULE=CASCADE \
              "fk0.DELETE_RULE=NO ACTION" fk0.PK_NAME=PK_People fk0.FK_NAME=FK_Pets_People fk0.DEFERRABILITY=3 \
              indexes=00000000 ix_rows=1 ix0.INDEX_NAME=PK_People ix0.UNIQUE=-1 ix0.CLUSTERED=0 ix0.TYPE=4 \
              ix0.ORDINAL_POSITION=1 ix0.COLUMN_NAME=Id ix0.COLLATION=1 ix0.CARDINALITY=2 \
              index_name_restriction=80040e53 provider_types=00000000 pt_rows=2 pt0.TYPE_NAME=INTEGER pt0.DATA_TYPE=3 \
              pt0.SEARCHABLE=3 pt0.IS_NULLABLE=-1 pt0.IS_LONG=0 pt1.TYPE_NAME=NVARCHAR pt1.DATA_TYPE=130 \
              pt1.LITERAL_PREFIX=N\' pt1.CASE_SENSITIVE=-1 pt1.SEARCHABLE=4; do
        printf '%s\n' "$out" | grep -qxF "$kv" && pass "$b: $kv" || fail "$b: $kv (got: $(printf '%s\n' "$out" | grep "^${kv%%=*}=" || echo none))"
    done
    for kv in schemas_listed2=11 views=00000000 vw_rows=1 vw0.TABLE_NAME=PeopleView "vw0.DESCRIPTION=adults only" \
              vw0.VIEW_DEFINITION=NULL procedure_parameters=00000000 pp_rows=3 pp0.PROCEDURE_NAME=GetPets \
              pp0.PARAMETER_NAME=@RETURN_VALUE pp0.ORDINAL_POSITION=0 pp0.PARAMETER_TYPE=4 \
              pp1.PARAMETER_NAME=@OwnerId pp1.ORDINAL_POSITION=1 pp1.PARAMETER_TYPE=1 pp1.PARAMETER_HASDEFAULT=0 \
              pp1.IS_NULLABLE=0 pp1.DATA_TYPE=3 pp1.NUMERIC_PRECISION=10 pp1.CHARACTER_MAXIMUM_LENGTH=NULL \
              pp2.PARAMETER_NAME=@Name pp2.PARAMETER_TYPE=2 pp2.PARAMETER_HASDEFAULT=-1 "pp2.PARAMETER_DEFAULT=N'x'" \
              pp2.IS_NULLABLE=-1 pp2.DATA_TYPE=130 pp2.CHARACTER_MAXIMUM_LENGTH=50 pp2.TYPE_NAME=NVARCHAR; do
        printf '%s\n' "$out" | grep -qxF "$kv" && pass "$b: $kv" || fail "$b: $kv (got: $(printf '%s\n' "$out" | grep "^${kv%%=*}=" || echo none))"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
