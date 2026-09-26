#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user's shell folders (patches/sg/0070).
#
# On the shared system prefix only administrators may write HKLM. shell32
# opened the machine's profile list for writing just to read it; for a
# standard user that failed, the profile path never expanded, and an empty
# Desktop was written to the user's "Shell Folders" cache -- after which
# ShellExecute of a shortcut failed and explorer showed no desktop icons.
# The gate makes the profile list read-only for users, as it is there, and
# asks for the Desktop with Administrators disabled.
#
#   WINE=/opt/wine-sg/bin/wine test/shellfolders-gate.sh
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
T=$(mktemp -d /var/tmp/sg-shellfolders.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/shellfolders-probe.exe" "$HERE/shellfolders-probe.c" -lshell32 -ladvapi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/shellfolders-probe.exe" run 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
get() { echo "$out" | sed -n "s/^$1=//p"; }

[ "$(get hklm_write)" = 5 ] && pass "the test user cannot write HKLM (access denied), as a standard user on the system prefix" \
    || fail "the test user can write HKLM ($(get hklm_write)): the gate would prove nothing"
[ "$(get hr)" = 0 ] && pass "SHGetFolderPath(CSIDL_DESKTOPDIRECTORY) succeeds for them" || fail "SHGetFolderPath: $(get hr)"
case "$(get path)" in *\\Desktop) pass "and names their Desktop: $(get path)" ;; *) fail "Desktop path: '$(get path)'" ;; esac
[ -n "$(get cached)" ] && [ "$(get cached)" = "$(get path)" ] && pass "the Shell Folders cache holds it, not an empty string" \
    || fail "Shell Folders cache: '$(get cached)'"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
