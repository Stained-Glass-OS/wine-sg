#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Web Account Manager (patches/sg/0512):
# Windows.Security.Authentication.Web.Core's WebAuthenticationCoreManager.
# Office's sign-in (OneAuth) asks it for the system's account providers; the
# class did not exist, so the sign-in ended with "class not registered"
# (0x80040154). No provider is installed here, and every lookup completes
# with none -- as Windows answers for a provider it lacks -- while a missing
# provider id and a lookup of a provider's accounts are invalid arguments.
#
#   WINE=/opt/wine-sg/bin/wine test/webauthcore-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-webauthcore.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/webauthcore-probe.c" -lruntimeobject -lole32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v factory)" = 0 ] && pass "the class activates with IWebAuthenticationCoreManagerStatics" || fail "factory: $(v factory)"
[ "$(v provider)" = "0 completed none" ] && pass "FindAccountProviderAsync completes with no provider" || fail "provider: $(v provider)"
[ "$(v authority)" = "0 completed none" ] && pass "FindAccountProviderAsync (with an authority) completes with none" || fail "authority: $(v authority)"
[ "$(v emptyid)" = "0x80070057 null" ] && pass "no provider id: E_INVALIDARG" || fail "emptyid: $(v emptyid)"
[ "$(v statics4)" = 0 ] && pass "IWebAuthenticationCoreManagerStatics4 too" || fail "statics4: $(v statics4)"
[ "$(v system)" = "0 completed none" ] && pass "FindSystemAccountProviderAsync completes with none" || fail "system: $(v system)"
[ "$(v allaccounts)" = "0x80070057 null" ] && pass "FindAllAccountsAsync without a provider: E_INVALIDARG" || fail "allaccounts: $(v allaccounts)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
