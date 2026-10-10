#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI data keys, token categories, token enumerators and object tokens
# (patches/sg/2841), on Xvfb: test/sapi-token-probe.c works on a scratch
# category in HKCU\Software\SGProbeSapi: ISpDataKey (binary, DWORD, enumeration,
# deletion) through categories and tokens, category data-key locations and the
# per-user default token, the enumerator (Skip, Reset, Clone, Sort,
# AddTokensFromDataKey/-TokenEnum), tokens (category lookup, attribute matching,
# storage files, Remove, UI) and the automation views late-bound through
# IDispatch. Before the patch these were FIXME stubs returning E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/sapi-token-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (sapi, -DSG_MUTANT_x): SAPITOK_GETDATA_SIZE (GetData reports no size),
# SAPITOK_DELETEKEY_FLAT (DeleteKey does not remove children), SAPITOK_SKIP_END
# (Skip past the end is S_OK), SAPITOK_CLONE_RESET (Clone restarts at 0),
# SAPITOK_SORT_FIRST (Sort ignores the first token), SAPITOK_CATEGORY_ID (the
# category is derived wrongly), SAPITOK_MATCH_ANY (MatchesAttributes always
# true), SAPITOK_STORAGE_REUSE (a registered storage name is not reused),
# SAPITOK_DEFAULT_USER (the user's default token is ignored), SAPITOK_ITEM_RANGE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapitoken.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sapi-token-probe.exe" "$HERE/sapi-token-probe.c" -lsapi -luuid -lole32 -loleaut32 -lshell32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sapi-token-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" sapi-token-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
