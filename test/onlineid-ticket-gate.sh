#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Security.Authentication.OnlineId (patches/sg/1474): Office's sign-in
# (OneAuth) builds an OnlineIdServiceTicketRequest and asks the system
# authenticator for a ticket; both were stubs, so the sign-in failed before it
# could fall back to another route. A request now keeps its service and policy,
# the authenticator keeps its ApplicationId, and GetTicketAsync completes at once
# with no identity and an error -- ERROR_NOT_FOUND, as a machine without a
# Microsoft account service refuses a ticket.
#
#   WINE=/opt/wine-sg/bin/wine test/onlineid-ticket-gate.sh
# Mutant: SG_MUTANT_ONLINEID_NO_TICKET_REQUEST (onlineid ticket.c).
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
T=$(mktemp -d /var/tmp/sg-onlineid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/onlineid-ticket-probe.c" -lruntimeobject -lole32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v reqfactory)" = 0 ] && pass "the request class activates with IOnlineIdServiceTicketRequestFactory" || fail "reqfactory: $(v reqfactory)"
[ "$(v create)" = "0 set" ] && pass "CreateOnlineIdServiceTicketRequest returns a request" || fail "create: $(v create)"
[ "$(v create_props)" = "0 0 service policy" ] && pass "its Service and Policy read back" || fail "create_props: $(v create_props)"
[ "$(v advanced)" = "0 set" ] && pass "CreateOnlineIdServiceTicketRequestAdvanced returns a request" || fail "advanced: $(v advanced)"
[ "$(v advanced_props)" = "0 0 service policy" ] && pass "its Service reads back and its Policy is empty" || fail "advanced_props: $(v advanced_props)"
[ "$(v authfactory)" = 0 ] && pass "OnlineIdSystemAuthenticator activates with its statics" || fail "authfactory: $(v authfactory)"
[ "$(v default)" = "0 set" ] && pass "OnlineIdSystemAuthenticator.Default returns an authenticator" || fail "default: $(v default)"
[ "$(v appid)" = "0 0 same" ] && pass "ApplicationId is put and read back" || fail "appid: $(v appid)"
[ "$(v getticket)" = "0 set" ] && pass "GetTicketAsync returns an operation" || fail "getticket: $(v getticket)"
[ "$(v status)" = 1 ] && pass "the operation is already Completed" || fail "status: $(v status)"
[ "$(v handler)" = "0 calls=1 status=1" ] && pass "a Completed handler is invoked at once, with Completed" || fail "handler: $(v handler)"
[ "$(v getresults)" = "0 set" ] && pass "GetResults gives a result" || fail "getresults: $(v getresults)"
[ "$(v result)" = "identity=null status=1 error=0x80070490" ] && pass "no identity, status Error, ExtendedError ERROR_NOT_FOUND" || fail "result: $(v result)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
