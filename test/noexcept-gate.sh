#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# vcruntime140's __C_specific_handler_noexcept (patches/sg/0510), the
# exception handler of a noexcept function holding __try blocks. It was a
# stub: each exception passing such a frame raised another into the same
# frame until the stack overflowed -- Word died signing in to Office. An
# unwind passing the frame continues; a C++ exception that would leave the
# function ends the process (std::terminate: exit code 3), once. An SEH
# exception that is not a C++ one passes the frame to the handlers outside it
# (0535): noexcept is about C++ exceptions -- Word raised its own SEH code
# through such a frame and hung in terminate.
#
#   WINE=/opt/wine-sg/bin/wine test/noexcept-gate.sh
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
T=$(mktemp -d /var/tmp/sg-noexcept.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/noexcept-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
U=$(timeout 60 "$WINE" "$T/probe.exe" unwind 2>/dev/null | tr -d '\r')
S=$(timeout 60 "$WINE" "$T/probe.exe" seh 2>/dev/null | tr -d '\r')
timeout 60 "$WINE" "$T/probe.exe" escape > "$T/escape" 2>&1
E=$?
echo "      $U / escape exit $E $(tr -d '\r' < "$T/escape" | grep -v '^[0-9a-f]*:' | head -2)"
[ "$S" = "seh 1" ] && pass "an SEH exception passes the frame (ExceptionContinueSearch, no terminate)" || fail "seh: $S"
[ "$U" = "unwind 1" ] && pass "an unwind passing the frame continues (ExceptionContinueSearch)" || fail "unwind: $U"
[ "$E" = 3 ] && ! grep -q "escape returned" "$T/escape" \
    && pass "an exception leaving the noexcept function ends the process (terminate)" || fail "escape: exit $E, $(cat "$T/escape")"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
