#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msdaps (patch 3190): the OLE DB proxies and stubs for IColumnsInfo, IAccessor::GetBindings, IDBProperties::
# GetPropertyInfo, ICommandWithParameters, ICommandProperties, IRowsetIdentity, IConvertType, ICommandPrepare,
# ITransactionLocal, IRowPosition and IMultipleResults, called from the main thread into a provider in an MTA
# thread (64- and 32-bit).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmsdaps-gate.sh
# Mutants (msdaps/usrmarshal.c): SG_MUTANT_MSD_NO_FIXUP, SG_MUTANT_MSD_NO_ERRINFO, SG_MUTANT_MSD_NO_STATUS
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastmsd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || { chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"; }' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe-$a.exe" "$HERE/fastmsdaps-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    timeout 120 "$WINE" "$T/probe-$a.exe" 2>/dev/null | tr -d '\r' > "$T/o"
    v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
    grep -q '^DONE' "$T/o" || { fail "the probe did not finish ($a): $(tr '\n' ' ' < "$T/o" | cut -c1-300)"; "$WINESERVER" -w; continue; }
    [ "$(v CI_GET)" = "00000000 2" ] && [ "$(v CI_NAMES)" = "id name" ] && [ "$(v CI_IDNAME)" = "2 idcol" ] && [ "$(v CI_IDPROP)" = "5 77" ] && [ "$(v CI_SHARED)" = 1 ] && [ "$(v CI_TYPES)" = "3 130 2" ] \
        && pass "IColumnsInfo::GetColumnInfo: names and column-id names come back as pointers into the one string buffer ($a)" || fail "column info ($a): $(grep '^CI_' "$T/o" | tr '\n' ' ')"
    [ "$(v CI_MAP)" = "00000000 11 22" ] && [ "$(v CI_MAP3)" = 00040eda ] && pass "IColumnsInfo::MapColumnIDs: result array and the success code ($a)" || fail "map ids ($a): $(v CI_MAP) $(v CI_MAP3)"
    [ "$(v AC_GET)" = "00000000 2 2" ] && [ "$(v AC_B)" = "1 8 3|2 16 130 3" ] && [ "$(v AC_BAD)" = 80040e00 ] && pass "IAccessor::GetBindings: flags, count and bindings; a bad handle's error ($a)" || fail "get bindings ($a): $(grep '^AC_' "$T/o" | tr '\n' ' ')"
    [ "$(v DP_GET)" = "00000000 1" ] && [ "$(v DP_DESC)" = "Data Source|(null)|Password" ] && [ "$(v DP_IDS)" = "1 2 3" ] && [ "$(v DP_SHARED)" = 1 ] \
        && pass "IDBProperties::GetPropertyInfo: descriptions as offsets (a NULL one stays NULL), one shared buffer ($a)" || fail "property info ($a): $(grep '^DP_' "$T/o" | tr '\n' ' ')"
    [ "$(v CP_GET)" = "00000000 2" ] && [ "$(v CP_NAMES)" = "@first @second 130 40" ] && [ "$(v CP_MAP)" = "00000000 2 1 0" ] && [ "$(v CP_SET)" = 00000000 ] \
        && pass "ICommandWithParameters: GetParameterInfo names, MapParameterNames, SetParameterInfo arrays ($a)" || fail "parameters ($a): $(grep '^CP_' "$T/o" | tr '\n' ' ')"
    [ "$(v CPR_SET)" = "00040eda 0 1" ] && [ "$(v CPR_GET)" = "00000000 1" ] && [ "$(v CPR_VAL)" = "5 1234" ] \
        && pass "ICommandProperties: per-property statuses come back; GetProperties ($a)" || fail "command properties ($a): $(grep '^CPR_' "$T/o" | tr '\n' ' ')"
    [ "$(v RI)" = "00000000 00000001" ] && [ "$(v CT)" = "00000000 00000001" ] && pass "IRowsetIdentity::IsSameRow and IConvertType::CanConvert keep S_OK / S_FALSE ($a)" || fail "ri/ct ($a): $(v RI) / $(v CT)"
    [ "$(v PR_OK)" = "00000000 00040ec9" ] && [ "$(v PR_FAIL)" = "80004005 prepare failed|fakeprov" ] && pass "ICommandPrepare: success codes, and a failure's error info crosses the apartment ($a)" || fail "prepare ($a): $(v PR_OK) / $(v PR_FAIL)"
    [ "$(v TL_START)" = "00000000 3" ] && [ "$(v TL_NOLVL)" = 00000000 ] && [ "$(v TL_BAD)" = 8004d008 ] && pass "ITransactionLocal::StartTransaction: nesting level out (or omitted), isolation error ($a)" || fail "transaction ($a): $(grep '^TL_' "$T/o" | tr '\n' ' ')"
    [ "$(v RP_GET)" = "00000000 5 9 0" ] && [ "$(v RP_SET)" = 00000000 ] && [ "$(v RP_CLEAR)" = 00040ec9 ] && pass "IRowPosition: GetRowPosition, SetRowPosition, ClearRowPosition ($a)" || fail "row position ($a): $(grep '^RP_' "$T/o" | tr '\n' ' ')"
    [ "$(v MR_COUNT)" = "00000000 41" ] && [ "$(v MR_ROWSET)" = "00000000 42 0" ] && pass "IMultipleResults::GetResult: rows affected with and without a rowset ($a)" || fail "multiple results ($a): $(grep '^MR_' "$T/o" | tr '\n' ' ')"
    [ "$(v OR_OPEN)" = "00040eda 1 0 1" ] && [ "$(v OR_BAD)" = "80040e37 0" ] && pass "IOpenRowset::OpenRowset: property statuses and the rowset come back; a missing table's error ($a)" || fail "open rowset ($a): $(grep '^OR_' "$T/o" | tr '\n' ' ')"
    "$WINESERVER" -w
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
