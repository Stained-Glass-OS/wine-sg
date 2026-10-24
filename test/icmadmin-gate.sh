#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscms G-ICM (6078): the system's colour profile store is machine configuration, so only administrators may
# change it, as on Windows (where the COLOR directory and the machine's ICM keys are administrators' only).
# InstallColorProfile, UninstallColorProfile, AssociateColorProfileWithDevice/DisassociateColorProfileFromDevice
# and the WCS calls of the system-wide scope (default profile, association, rendering intent) gave anyone a
# machine-wide change; now a caller whose token is not an enabled member of Administrators gets
# ERROR_ACCESS_DENIED. The current-user scope stays open to everyone.
# The probe makes every change as the administrator it is, then impersonating its own token with the
# Administrators group deny-only (a standard user's token).
#   WINE=/opt/wine-sg/bin/wine test/icmadmin-gate.sh   (mutant SG_MUTANT_ICM_ANYONE)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icmadmin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/icmadmin-probe.c" -lmscms -ladvapi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>"${ICM_LOG:-/dev/null}" </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect admin 1 "the probe runs as an administrator"
expect admin_install "1 0" "an administrator installs a profile"
expect admin_associate_system "1 0" "and associates one system-wide"
expect admin_intent_system "1 0" "and sets the system's rendering intent"
expect admin_associate_legacy "1 0" "and AssociateColorProfileWithDevice"
expect admin_uninstall "1 0" "and uninstalls (deletes) the profile"
expect admin_file_gone 1 "the profile is gone"
expect restricted 1 "the impersonated token is no administrator's"
expect user_install "0 5" "a standard user cannot install: ERROR_ACCESS_DENIED"
expect user_associate_system "0 5" "nor associate system-wide"
expect user_intent_system "0 5" "nor set the system's rendering intent"
expect user_associate_legacy "0 5" "nor AssociateColorProfileWithDevice"
expect user_uninstall "0 5" "nor uninstall an installed profile"
expect user_file_kept 1 "the administrator's profile is still there"
expect user_intent_user "1 0" "the user's own rendering intent stays open"
expect user_associate_user "1 0" "and the user's own associations"
expect admin_intent_user "1 0" "(the administrator's too)"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
