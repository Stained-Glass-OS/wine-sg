#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript batch b1: calls of instances, host objects in Object.*, variant conversions, IActiveScript thread/interrupt/clone/typelib/scriptlet,
# IActiveScriptProperty, IDispatchEx property flags and internal names.
#
#   WINE=/opt/wine-sg/bin/wine test/fastjsb1-gate.sh   (mutants SG_MUTANT_JSB1_*)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastjsb1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastjsb1-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1=//p" "$T/o" | head -1; }
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: got '$(v "$1")'"; }
chk CALL_BOOL "5002" "call bool"
chk CALL_BOOL2 "5002" "call bool2"
chk CALL_RE "5002" "call re"
chk CALL_ERR "5002" "call err"
chk ENUM_CALL "object" "enum call"
chk ENUM_BAD "451" "enum bad"
chk AX_CALL "0" "ax call"
chk AX_NOARG "5" "ax noarg"
chk H_KEYS "alpha,beta" "h keys"
chk H_NAMES "alpha,beta" "h names"
chk H_PIE "true,false,false,false" "h pie"
chk H_GOPD "22,true,true,true," "h gopd"
chk H_DEFPROP "true,33" "h defprop"
chk H_DEFPROPS "44,55" "h defprops"
chk H_DEFACC "445" "h defacc"
chk H_DESC_HOST "66" "h desc host"
chk H_EXT "true,false,false,true,true,true,true" "h ext"
chk D_KEYS "0" "d keys"
chk D_GOPD "1" "d gopd"
chk D_DEF "445" "d def"
chk D_PIE "false,false" "d pie"
chk O_CREATE_BAD "5007" "o create bad"
chk O_CREATE_BAD2 "5007" "o create bad2"
chk V_TYPEOF "date,unknown,unknown,unknown,unknown" "v typeof"
chk V_DATE_NUM "-1" "v date num"
chk V_I8_NUM "12345678901" "v i8 num"
chk V_I8_STR "12345678901" "v i8 str"
chk V_DEC "1.5,1.5" "v dec"
chk V_I8_BOOL "true,true,true" "v i8 bool"
chk V_OBJ "object,12345678902,object,1900" "v obj"
chk V_ERR "err 5" "v err"
chk V_JSON "40,[12345678901," "an engine-held date and 64 bit integer in JSON.stringify"
chk V_DELETE "true,false" "v delete"
chk STATE_IN "1" "state in"
chk CALL_VBA "5002" "call vba"
chk CALL_MAP "5002" "call map"
chk CALL_SET "5002" "call set"
chk CALL_WMAP "5002" "call wmap"
chk CT_UI1 "200" "ct ui1"
chk CT_I1 "-56" "ct i1"
chk CT_UI4 "200" "ct ui4"
chk CT_INT "200" "ct int"
chk CT_I8 "200" "ct i8"
chk CT_CY "2000000" "ct cy"
chk CT_I8FROM "vt3" "ct i8from"
chk MP_FN "00000000 2555" "mp fn"
chk MP_NUM "00000000 2a55" "mp num"
chk MP_RO "00000000 2a69" "mp ro"
chk MP_ONLYCALL "00000000 100" "mp onlycall"
chk MP_BADFLAG "80070057" "mp badflag"
chk INT_ENSURE "00000000" "int ensure"
chk NORM_ENSURE "00000000" "norm ensure"
chk BAD_GRFDEX "80070057" "bad grfdex"
chk INT_ENUM "shownNormal" "int enum"
chk TID "00000000 1" "tid"
chk TID_NULL "80004003" "tid null"
chk TID2 "00000000 1" "tid2"
chk TID2_BAD "80070057" "tid2 bad"
chk TS_OUT "00000000 0" "ts out"
chk TS_BAD "80070057" "ts bad"
chk INT_NULL "80004003" "int null"
chk INT_BADFLAGS "80070057" "int badflags"
chk INT_BADTID "80070057" "int badtid"
chk PR_NAME "00000000 JScript" "pr name"
chk PR_MAJOR "00000000 5" "pr major"
chk PR_MINOR "00000000 8" "pr minor"
chk PR_VER "00000000 503" "pr ver"
chk PR_ABBR_SET "00000000" "pr abbr set"
chk PR_ABBR "00000000 1" "pr abbr"
chk PR_UNKNOWN "80004001" "pr unknown"
chk ST0 "2" "st0"
chk ST_DISC "00000000 3" "st disc"
chk ST_INIT "00000000 5" "st init"
chk ST_START "00000000 1" "st start"
chk ST_CLOSED "80070057" "st closed"
chk TL_BEFORE "undefined" "tl before"
chk TL_ADD "00000000" "tl add"
chk TL_AFTER "number,1,8,-1" "tl after"
chk TL_DELETE "0" "tl delete"
chk TL_BAD "1" "tl bad"
chk TL_BADFLAGS "80070057" "tl badflags"
chk SL_FN "00000000 h1" "sl fn"
chk SL_FN_RUN "7" "sl fn run"
chk SL_EVT "00000000 hostex_onclick handler=onclick" "sl evt"
chk SL_CALL "00000000 9" "sl call"
chk SL_NONAME "80070057" "sl noname"
chk SL_BADITEM "80070057" "sl baditem"
chk SL_SYNTAX "1" "sl syntax"
chk CL_NULL "80004003" "cl null"
chk CL "00000000" "cl"
chk CL_STATE0 "0" "cl state0"
chk CL_SITE "00000000" "cl site"
chk CL_STATE1 "5" "cl state1"
chk CL_ITEM "11" "cl item"
chk CL_CODE "42" "cl code"
chk CL_TYPELIB "8" "cl typelib"
chk INTR_ABORT_after "0,0" "intr abort after"
chk INTR_RAISE_after "1,80041234" "intr raise after"
chk INTR_NESTED_after "undefined" "intr nested after"
chk INTR_STALE "2" "intr stale"
chk INTR_ABORT "80020101 int=00000000 took=short" "an interrupt aborts a looping script with the error from the EXCEPINFO reported"
chk INTR_RAISE "00000000 int=00000000 took=short" "an interrupt with RAISEEXCEPTION is a catchable exception"
chk INTR_NESTED "80020101 int=00000000 took=short" "an abort unwinds nested calls past catch handlers"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
