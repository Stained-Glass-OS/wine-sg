#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Web Account Manager's work and school provider (patches/sg/1479):
# Windows.Security.Authentication.Web.Core's WebAuthenticationCoreManager.
# Office's sign-in (OneAuth) asks it for an account provider and then for
# tokens. A provider is found (any non-empty id; an empty id and a lookup of
# a missing provider's accounts stay E_INVALIDARG), and the token requests
# (GetTokenSilentlyAsync, RequestTokenAsync, the WithWebAccount variants) go to
# a native sign-in program, /usr/bin/sg-wam-msal or the Unix path in
# SG_WAM_HELPER, which answers with one SGWAM-RESULT:{json} line. This gate
# points SG_WAM_HELPER at test/webauthcore-helper.sh, a stub that picks its
# answer by the request's login_hint, and checks that the answer arrives as
# Windows' own would: the status (UserInteractionRequired, UserCancel,
# Success), the token, the account, and the response properties MSAL-based
# clients read -- TokenExpiresOn in seconds since 1601 (MSAL rejects
# anything below 11644473600) and the wamcompat_* names.
#
#   WINE=/opt/wine-sg/bin/wine test/webauthcore-gate.sh
# Mutants: SG_MUTANT_WAM_NO_EXPIRES_ON, SG_MUTANT_WAM_NO_WAMCOMPAT
# (windows.security.authentication.web.core broker.c).
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
export SG_WAM_HELPER="$HERE/webauthcore-helper.sh" SG_WAM_REQLOG="$T/requests"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/webauthcore-probe.c" -lruntimeobject -lole32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
: > "$T/requests"
timeout 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out" | head -n 1; }
p() { sed -n "s/^prop $1 //p" "$T/out" | head -n 1; }
[ "$(v factory)" = 0 ] && pass "the class activates with IWebAuthenticationCoreManagerStatics" || fail "factory: $(v factory)"
[ "$(v provider)" = "0 completed some" ] && pass "FindAccountProviderAsync (with an authority) completes with a provider" || fail "provider: $(v provider)"
[ "$(v providerid)" = "https://login.microsoft.com" ] && [ "$(v providerauthority)" = organizations ] \
    && pass "the provider's Id and Authority read back" || fail "provider id/authority: $(v providerid) / $(v providerauthority)"
[ "$(v noauthority)" = "0 completed some" ] && pass "FindAccountProviderAsync without an authority completes with a provider" || fail "noauthority: $(v noauthority)"
[ "$(v emptyid)" = "0x80070057 null" ] && pass "no provider id: E_INVALIDARG" || fail "emptyid: $(v emptyid)"
[ "$(v system)" = "0 completed some" ] && pass "FindSystemAccountProviderAsync completes with a provider" || fail "system: $(v system)"
[ "$(v allaccounts)" = "0x80070057 null" ] && pass "FindAllAccountsAsync without a provider: E_INVALIDARG" || fail "allaccounts: $(v allaccounts)"
[ "$(v silent)" = "0 3" ] && pass "silent request, helper wants interaction: UserInteractionRequired" || fail "silent: $(v silent)"
[ "$(v interactive)" = "0 0" ] && [ "$(v responses)" = 1 ] && pass "interactive request: Success with one response" || fail "interactive: $(v interactive), responses $(v responses)"
[ "$(v token)" = "tok-ABC123" ] && pass "the response's Token is the helper's access_token" || fail "token: $(v token)"
[ "$(v accountid)" = "uid.utid" ] && [ "$(v username)" = "jane@contoso.com" ] \
    && pass "the response's WebAccount has the helper's home_account_id and username" || fail "account: $(v accountid) / $(v username)"
[ "$(v expireson)" = ok ] && pass "TokenExpiresOn is seconds since 1601 (> 11644473600)" || fail "TokenExpiresOn: $(v expireson)"
[ "$(p expires_in)" = 3599 ] && pass "expires_in property" || fail "expires_in: $(p expires_in)"
[ "$(p wamcompat_id_token)" = "idtok.payload.sig" ] && [ "$(p wamcompat_client_info)" = "eyJ1aWQiOiJ4In0" ] && [ "$(p wamcompat_scopes)" = "User.Read openid" ] \
    && pass "wamcompat_id_token, wamcompat_client_info and wamcompat_scopes properties" \
    || fail "wamcompat: $(p wamcompat_id_token) / $(p wamcompat_client_info) / $(p wamcompat_scopes)"
[ "$(v silentacct)" = "0 3" ] && [ "$(v interactiveacct)" = "0 0" ] && pass "the WithWebAccount variants answer the same" || fail "with account: $(v silentacct) / $(v interactiveacct)"
[ "$(v silentok)" = "0 0" ] && pass "silent request, helper succeeds: Success" || fail "silentok: $(v silentok)"
[ "$(v denied)" = "0 1" ] && pass "helper error access_denied: UserCancel" || fail "denied: $(v denied)"
[ "$(v cancelled)" = "0 1" ] && pass "helper error description with \"cancel\": UserCancel" || fail "cancelled: $(v cancelled)"
[ "$(v cancelledcase)" = "0 1" ] && pass "helper description \"User Cancelled\" (any case): UserCancel" || fail "cancelledcase: $(v cancelledcase)"
[ "$(v silentacctnohint)" = "0 0" ] && [ "$(v interactiveacctnohint)" = "0 0" ] || fail "no-hint account requests: $(v silentacctnohint) / $(v interactiveacctnohint)"
[ "$(grep -c '"account_id":"uid.utid".*"login_hint":"jane@contoso.com"\|"login_hint":"jane@contoso.com".*"account_id":"uid.utid"' "$T/requests")" -ge 2 ] \
    && grep -q '"mode":"silent".*"login_hint":"jane@contoso.com"' "$T/requests" && grep -q '"mode":"interactive".*"login_hint":"jane@contoso.com"' "$T/requests" \
    && pass "the WithWebAccount variants send account_id and, without a LoginHint, the account's UserName as login_hint" \
    || fail "account request not in the helper's input: $(grep -c account_id "$T/requests") with account_id"
grep -q '"login_hint":"loginhint-marker@contoso.com"' "$T/requests" && pass "the LoginHint property reaches the helper as login_hint" || fail "login_hint not in the helper's request: $(tail -n 1 "$T/requests")"
grep -q '"authority":"https://login.microsoftonline.com/organizations"' "$T/requests" && grep -q '"mode":"interactive"' "$T/requests" && grep -q '"mode":"silent"' "$T/requests" \
    && pass "the helper's request carries mode and authority" || fail "request shape: $(tail -n 1 "$T/requests")"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
