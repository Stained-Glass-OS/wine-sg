#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# setx.exe (patches/sg/5520): command line mode, registry mode (/k), /m, the
# 1024 character limit, a variable stored with references keeps its type,
# and the syntax errors and the unsupported forms exit 1.
#
#   WINE=/opt/wine-sg/bin/wine test/setx-gate.sh
# Mutants (programs/setx/main.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_SETX_NOTRUNC (no 1024 limit), SG_MUTANT_SETX_RESETS_TYPE (always REG_SZ).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-setx.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

setx() { DISPLAY= timeout -s KILL 60 "$WINE" setx "$@" >"$T/out" 2>"$T/err"; RET=$?; tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"; }
# value NAME [KEY]: "TYPE<TAB>DATA" of a value
getval() { DISPLAY= timeout -s KILL 60 "$WINE" reg query "${2:-HKCU\\Environment}" /v "$1" 2>/dev/null | tr -d '\r' | awk -v n="$1" '$1==n {t=$2; $1="";$2=""; sub(/^  */,""); print t "\t" $0}'; }

setx SGVAR hello
[ $RET = 0 ] && grep -q "SUCCESS" "$T/o" && [ "$(getval SGVAR)" = "REG_SZ	hello" ] \
    && pass "setx NAME VALUE stores a REG_SZ in HKCU\\Environment and says so" || fail "plain: ret=$RET $(cat "$T/o" "$T/e") / $(getval SGVAR)"
setx SGSPACE "two words here"
[ $RET = 0 ] && [ "$(getval SGSPACE)" = "REG_SZ	two words here" ] && pass "a value with spaces" || fail "spaces: $RET / $(getval SGSPACE)"
setx SGMACH machine /m
[ $RET = 0 ] && [ "$(getval SGMACH 'HKLM\System\CurrentControlSet\Control\Session Manager\Environment')" = "REG_SZ	machine" ] \
    && pass "/m writes the system environment" || fail "/m: $RET $(cat "$T/e")"

LONG=$(awk 'BEGIN{for(i=0;i<1100;i++)printf "x"}')
setx SGLONG "$LONG"
GOT=$(getval SGLONG | cut -f2 | tr -d '\n' | wc -c)
[ $RET = 0 ] && [ "$GOT" = 1024 ] && grep -q "truncated to 1024" "$T/e" \
    && pass "a value over 1024 characters is cut to 1024, with a warning" || fail "truncation: ret=$RET stored=$GOT $(cat "$T/e")"

"$WINE" reg add 'HKCU\Environment' /v SGEXP /t REG_EXPAND_SZ /d '%SystemRoot%\old' /f >/dev/null 2>&1
setx SGEXP newvalue
[ "$(getval SGEXP)" = "REG_EXPAND_SZ	newvalue" ] && pass "a variable stored with references keeps REG_EXPAND_SZ" || fail "keep type: $(getval SGEXP)"

"$WINE" reg add 'HKCU\Software\SgSetx' /v Text /t REG_SZ /d world /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\SgSetx' /v Num /t REG_DWORD /d 255 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\SgSetx' /v Exp /t REG_EXPAND_SZ /d '%SystemRoot%' /f >/dev/null 2>&1
setx SGK /k 'HKEY_CURRENT_USER\Software\SgSetx\Text'
[ $RET = 0 ] && [ "$(getval SGK)" = "REG_SZ	world" ] && pass "/k takes the value from a registry key" || fail "/k: $RET $(cat "$T/e") / $(getval SGK)"
setx SGN /k 'HKEY_CURRENT_USER\Software\SgSetx\Num'
[ "$(getval SGN)" = "REG_SZ	0xff" ] && pass "/k of a REG_DWORD gives it in hexadecimal" || fail "/k dword: $(getval SGN)"
setx SGE /k 'HKEY_CURRENT_USER\Software\SgSetx\Exp'
case "$(getval SGE)" in "REG_SZ	"[A-Za-z]:*|"REG_SZ	"/*|"REG_SZ	"*[Ww]indows*) pass "/k of a REG_EXPAND_SZ gives the expanded value" ;; *) fail "/k expand: $(getval SGE)" ;; esac
setx SGMISS /k 'HKEY_CURRENT_USER\Software\SgSetx\Nope'
[ $RET = 1 ] && pass "/k of a missing value fails" || fail "/k missing: $RET"

setx; R0=$RET
setx ONLYNAME; R1=$RET
setx A B C; R2=$RET
setx /s somehost A B; R3=$RET
setx A /f file.txt /a 1,1; R4=$RET
[ "$R0$R1$R2$R3$R4" = 11111 ] && pass "a missing or extra parameter, /s and file mode exit 1" || fail "errors: $R0 $R1 $R2 $R3 $R4"

[ "$RC" = 0 ] && echo "setx-gate: all passed" || echo "setx-gate: FAILURES"
exit $RC
