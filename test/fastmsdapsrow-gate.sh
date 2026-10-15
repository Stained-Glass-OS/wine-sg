#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msdaps row_server.c and ICommand::Execute with parameters (patch 3191): rowset proxy RestartPosition, Compare,
# GetRowsByBookmark, Hash, GetReferencedRowset, GetSpecification, AddRefAccessor; IRow::GetSourceRowset / Open;
# IBindResource::Bind for rows and other objects; DBPARAMS marshaled as an RMTPACK. 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmsdapsrow-gate.sh
# Mutants (msdaps): SG_MUTANT_MSD_BM_OFFSET, _CMP_LEN, _RESTART, _NO_WRAP (row_server.c);
#                   _NO_STRIDE, _NO_OUTPUT, _BYREF_LEN, _BIND_OTHER (usrmarshal.c)
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
T=$(mktemp -d /var/tmp/sg-fastmsr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || { chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"; }' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe-$a.exe" "$HERE/fastmsdapsrow-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    timeout 120 "$WINE" "$T/probe-$a.exe" 2>/dev/null | tr -d '\r' > "$T/o"
    v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
    grep -q '^DONE' "$T/o" || { fail "the probe did not finish ($a): $(tr '\n' ' ' < "$T/o" | cut -c1-300)"; "$WINESERVER" -w; continue; }
    [ "$(v RS_RESTART)" = 00040ec7 ] && pass "IRowset::RestartPosition: the provider's success code comes back ($a)" || fail "restart ($a): $(v RS_RESTART)"
    [ "$(v RS_CMP1)" = "00000000 0 3 1" ] && [ "$(v RS_CMP2)" = "00000000 1 2 4" ] && [ "$(v RS_CMP3)" = "00000000 2 1 3" ] \
        && pass "IRowsetLocate::Compare: both bookmarks arrive with their own lengths, the result comes back ($a)" || fail "compare ($a): $(grep '^RS_CMP' "$T/o" | tr '\n' ' ')"
    [ "$(v RS_GRBB)" = "00000000 71 104 302 0 0 0" ] && [ "$(v RS_GRBB_NOST)" = "00000000 71 104 302" ] && [ "$(v RS_GRBB_ZERO)" = 00000000 ] \
        && pass "GetRowsByBookmark: bookmarks of 1, 4 and 2 bytes in one buffer, statuses optional, none requested is S_OK ($a)" || fail "by bookmark ($a): $(grep '^RS_GRBB' "$T/o" | tr '\n' ' ')"
    [ "$(v RS_GRBB_BAD)" = "00040eda 0 12 0" ] && pass "GetRowsByBookmark: a bad bookmark's row status and DB_S_ERRORSOCCURRED ($a)" || fail "by bookmark bad ($a): $(v RS_GRBB_BAD)"
    [ "$(v RS_HASH)" = "00000000 218 314 932 0" ] && [ "$(v RS_HASH_NOST)" = "00000000 218 314 932" ] && pass "IRowsetLocate::Hash: hash values, statuses optional ($a)" || fail "hash ($a): $(grep '^RS_HASH' "$T/o" | tr '\n' ' ')"
    [ "$(v RS_ACC)" = "00000000 6" ] && [ "$(v RS_ACC_BAD)" = "80040e00 0" ] && [ "$(v RS_ACC_NULL)" = 00000000 ] \
        && pass "IAccessor::AddRefAccessor: reference count out, bad handle error, count pointer optional ($a)" || fail "addref accessor ($a): $(grep '^RS_ACC' "$T/o" | tr '\n' ' ')"
    [ "$(v RS_REF)" = "00000000 1" ] && [ "$(v RS_REF_CMP)" = "00000000 1 2" ] && [ "$(v RS_REF_BAD)" = "80040e0a 0" ] \
        && pass "IRowsetInfo::GetReferencedRowset: a usable rowset proxy for the referenced rowset; the error and NULL otherwise ($a)" || fail "referenced rowset ($a): $(grep '^RS_REF' "$T/o" | tr '\n' ' ')"
    [ "$(v RS_SPEC)" = "00000000 1" ] && [ "$(v RS_SPEC_USE)" = "00000000 1" ] && pass "IRowsetInfo::GetSpecification: the command object comes back usable ($a)" || fail "specification ($a): $(grep '^RS_SPEC' "$T/o" | tr '\n' ' ')"
    [ "$(v BIND_ROW)" = "00000000 1" ] && [ "$(v ROW_SRC)" = "00000000 1 7" ] && [ "$(v ROW_SRC_CMP)" = "00000000 1 3" ] \
        && [ "$(v ROW_SRC_GRBB)" = "00000000 71 104" ] && [ "$(v ROW_SRC_NOPTR)" = "00000000 7" ] && [ "$(v ROW_SRC_NOHROW)" = "00000000 1" ] && [ "$(v ROW_SRC_INFO)" = "00000000 1" ] \
        && pass "IRow::GetSourceRowset: rowset proxy and the row handle, either out parameter optional, any interface ($a)" || fail "source rowset ($a): $(grep '^ROW_SRC\|^BIND_ROW' "$T/o" | tr '\n' ' ')"
    [ "$(v ROW_OPEN_ROW)" = "00000000 1" ] && [ "$(v ROW_OPEN_ROW_SRC)" = "00000000 1" ] && [ "$(v ROW_OPEN_RS)" = "00000000 1" ] \
        && pass "IRow::Open: a row and a rowset come back as proxies ($a)" || fail "row open ($a): $(grep '^ROW_OPEN' "$T/o" | tr '\n' ' ')"
    [ "$(v ROW_OPEN_STREAM)" = "00000000 1 0" ] && [ "$(v ROW_OPEN_STREAM_USE)" = "00000000 1" ] && [ "$(v BIND_OTHER)" = "00000000 1 0" ] && [ "$(v BIND_ROWSET)" = "00000000 1" ] \
        && pass "IRow::Open and IBindResource::Bind for other objects: not aggregated, marshaled as the interface asked for ($a)" || fail "other objects ($a): $(grep '^ROW_OPEN_STREAM\|^BIND_' "$T/o" | tr '\n' ' ')"
    [ "$(v CMD_ACC)" = "00000000 1" ] && [ "$(v CMD_EXEC)" = "00000000 33 sets 2" ] \
        && [ "$(v CMD_IN0)" = "21 first byref-zero 20 3:77" ] && [ "$(v CMD_IN1)" = "8 second-bstr byref-one! 20 8:5" ] \
        && pass "ICommand::Execute with parameters: two parameter sets arrive (integer, BSTR, VARIANT, by-reference string with length) ($a)" || fail "execute params in ($a): $(grep '^CMD_ACC\|^CMD_EXEC\|^CMD_IN' "$T/o" | tr '\n' ' ')"
    [ "$(v CMD_STREAM)" = 1 ] && [ "$(v CMD_STREAM_DATA)" = "5 hello" ] && pass "ICommand::Execute: an interface parameter (a stream) is usable by the provider ($a)" || fail "execute stream ($a): $(grep '^CMD_STREAM' "$T/o" | tr '\n' ' ')"
    [ "$(v CMD_OUT0)" = "42 0 out-bstr out-ref 14 105" ] && [ "$(v CMD_OUT1)" = "16 0 out-bstr out-ref 14 106" ] && [ "$(v CMD_IO0)" = "io:inout" ] && [ "$(v CMD_IO1)" = "io:two" ] \
        && pass "ICommand::Execute: output and input/output parameters come back in every set (integer, status, BSTR, by-reference string) ($a)" || fail "execute params out ($a): $(grep '^CMD_OUT\|^CMD_IO' "$T/o" | tr '\n' ' ')"
    [ "$(v CMD_NOPARAM)" = "00000000 1" ] && [ "$(v CMD_BADACC)" = 80040e00 ] && pass "ICommand::Execute: without parameters or rowset pointer; a bad accessor's error ($a)" || fail "execute plain ($a): $(grep '^CMD_NOPARAM\|^CMD_BADACC' "$T/o" | tr '\n' ' ')"
    "$WINESERVER" -w
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
