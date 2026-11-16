#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# LPC shared sections (7810, K-LPC): NtConnectPort and NtAcceptConnectPort refused a section
# (STATUS_NOT_SUPPORTED). Now a client's section (LPC_SECTION_WRITE) is mapped by the client and by the
# server, the same range, and a server's section by both as well:
#  - the client fills its section before connecting; the server's LPC_SECTION_READ view shows it;
#  - what the server writes into that view before completing, the client finds in its own view;
#  - the client's TargetViewBase is where the server mapped its section (a 32-bit client: the low half);
#  - the server's own section reaches the client's LPC_SECTION_READ with what the server wrote;
#  - for a 64-bit and a 32-bit client in another process (32-bit views below 4 GB).
#   WINE=/opt/wine-sg/bin/wine test/lpcview-gate.sh   (mutant SG_MUTANT_LPC_NO_REMOTE_VIEW, ntdll unix/sync.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-lpcview.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/lpcview-probe.exe" "$HERE/lpcview-probe.c" -lntdll || { fail "the probe did not build"; exit 1; }
"$MINGW32" -O2 -o "$T/lpcview-probe32.exe" "$HERE/lpcview-probe.c" -lntdll || { fail "the 32-bit probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/lpcview-probe.exe" "$T/lpcview-probe32.exe" "$C/"
(cd "$C" && timeout -s KILL 120 "$WINE" lpcview-probe.exe > "$T/out" 2>"${LPC_LOG:-/dev/null}" </dev/null)
out=$(tr -d '\r' <"$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
for w in W64 W32; do
    expect $w.Accept "0x00000000 1 1" "$w: NtAcceptConnectPort maps the server's section and the client's"
    expect $w.ClientView 1 "$w: the client's view as it filled it, in the server"
    expect $w.Complete 0x00000000 "$w: NtCompleteConnectPort"
    expect $w.Child 0 "$w: the client's TargetViewBase, the server's writes in both views (exit code 0)"
done
expect Done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
