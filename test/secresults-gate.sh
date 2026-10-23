#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# advapi32/kernelbase: the SEC-ACL/SEC-AUDIT subsets DECISIONS-2 approves (6057):
#  - AccessCheckByTypeResultList: each object type's own result -- an object ACE for a type reaches
#    its subtree, a deny for a child does not reach its parent or siblings (A > B > (C, E), A > D,
#    allowed on B, denied on C: B and E granted, A, C and D denied); plain ACEs everywhere; no list:
#    ERROR_INVALID_PARAMETER;
#  - the access checks that audit: SeAuditPrivilege in the caller's own token (an administrator
#    lacks it: ERROR_PRIVILEGE_NOT_HELD), AUDIT_ALLOW_NO_PRIVILEGE checks without it, the client is
#    the impersonated one (ERROR_NO_IMPERSONATION_TOKEN when there is none); no audit is generated;
#  - BuildImpersonateTrustee/ExplicitAccessWithName, GetMultipleTrustee/Operation;
#  - CredMarshal/UnmarshalCredential and CredIsMarshaledCredential of binary blob credentials (SEC-CRED);
#  - TreeSetNamedSecurityInfoA down a tree of keys, GetInheritanceSourceW/A (an inherited ACE's ancestor
#    three levels up, past a closer ancestor's matching ACE that is not inheritable), FreeInheritedFromArray.
#   WINE=/opt/wine-sg/bin/wine test/secresults-gate.sh   (mutants SG_MUTANT_RESULTLIST_SKIP_DENY (a wrong
#   grant), SG_MUTANT_RESULTLIST_ALL_TYPES, SG_MUTANT_RESULTLIST_NO_INHERIT, SG_MUTANT_AUDIT_NO_PRIVILEGE_CHECK, SG_MUTANT_INHERIT_ANY_ACE,
#   SG_MUTANT_CRED_BLOB_SIZE_ONLY)
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
T=$(mktemp -d /var/tmp/sg-secresults.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/secresults-probe.exe" "$HERE/secresults-probe.c" -ladvapi32 || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/secresults-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" secresults-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect ResultList "1 0/5 1/0 0/5 1/0 0/5" "AccessCheckByTypeResultList: allowed on B reaches E, denied on C stays on C"
expect ResultListPlain "1 20000/0 20000/0" "plain ACEs give every object type their rights"
expect ResultListNoTypes "0 87" "no object type list: ERROR_INVALID_PARAMETER"
expect AuditAlarm "0 1314" "AccessCheckAndAuditAlarm needs SeAuditPrivilege"
expect ByTypeAuditNoImpersonation "0 1309" "with AUDIT_ALLOW_NO_PRIVILEGE, the check needs a client to impersonate"
expect ByTypeAudit "1 20000 1 0" "and checks the client, generating no audit"
expect ByTypeAuditNoPrivilege "0 1314" "without AUDIT_ALLOW_NO_PRIVILEGE the privilege is needed"
expect Trustee "1 1 1 Everyone 1" "impersonate trustees and explicit accesses, GetMultipleTrustee(Operation)"
expect SetExplicit 0 "a key gets an explicit, not inheritable ACE"
expect TreeSetA 0 "TreeSetNamedSecurityInfoA gives an inheritable ACE to the tree"
expect InheritanceSource "0 3 CURRENT_USER\\Software\\SGInh" "GetInheritanceSourceW finds the ancestor three levels up"
expect InheritanceSourceA "0 3 CURRENT_USER\\Software\\SGInh" "and GetInheritanceSourceA"
expect BlobCredential "1 1 1 1 1 0" "SEC-CRED: a binary blob credential marshals, unmarshals and is recognized; a bad one is not"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
