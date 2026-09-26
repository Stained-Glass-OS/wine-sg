#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Objects created without a security descriptor have an owner and a group
# (patches/sg/0082).
#
# On Windows, a pipe, event, mutex, semaphore or section created with no
# descriptor is owned by its creator (the token's owner and primary group);
# Wine reported none. The Cygwin/MSYS runtime (Git Bash's terminal) takes a
# pty's owner from its pipes and crashed on the NULL. Access must not change:
# no DACL is added, and a process without Administrators still opens them.
# And only those kinds: a registry key (a first version of the patch gave
# keys owner-only descriptors -- open to everyone, inherited into the saved
# registry, and it broke the session's display links) keeps its defaults.
#
#   WINE=/opt/wine-sg/bin/wine test/objowner-gate.sh
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
T=$(mktemp -d /var/tmp/sg-objowner.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/objowner-probe.exe" "$HERE/objowner-probe.c" -lntdll -ladvapi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/objowner-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
for kind in anonymous-pipe named-pipe-server named-pipe-client event named-event mutex semaphore named-section; do
    line=$(printf '%s\n' "$out" | grep "^$kind ")
    case "$line" in
        *"status=0 owner=creator group=creator dacl=absent") pass "$kind: owned by its creator, no DACL added" ;;
        *) fail "$kind: $line" ;;
    esac
done
line=$(printf '%s\n' "$out" | grep '^registry-key ')
case "$line" in
    *"dacl=present") pass "a registry key created without a descriptor keeps Wine's key defaults (a DACL), not an owner-only one" ;;
    *) fail "registry key defaults changed: $line" ;;
esac
case "$out" in *"restricted-open event=1 section=1"*) pass "a process without Administrators still opens them with full access" ;;
    *) fail "access changed: $(printf '%s\n' "$out" | grep restricted)" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
