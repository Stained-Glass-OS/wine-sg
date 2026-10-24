#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The SAPI voice's name (6080): the eSpeak NG voice showed as "Wine Default Voice" (qa5, release 539), with
# Female/Wine attributes it did not have. Its token is now named "Stained Glass Voice (eSpeak NG)" (description
# and the 409 name), vendor eSpeak NG, gender Male (eSpeak NG's default voice), adult, US English (409, what
# it speaks unless a <lang> tag asks for another). The token's ID is unchanged, as programs may have stored
# it. No "Windows", "Microsoft" or "Wine" in what the user sees.
#   WINE=/opt/wine-sg/bin/wine test/voicename-gate.sh
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
T=$(mktemp -d /var/tmp/sg-voicename.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/voicename-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect voice 1 "SpVoice"
expect token 1 "its voice token"
expect id 'HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\Wine Default Voice' "the token ID is unchanged"
expect description "Stained Glass Voice (eSpeak NG)" "the voice's name"
expect description409 "Stained Glass Voice (eSpeak NG)" "its US English name"
expect Name "Stained Glass Voice (eSpeak NG)" "the Name attribute"
expect Vendor "eSpeak NG" "the vendor: eSpeak NG"
expect Gender Male "the gender eSpeak NG's default voice has"
expect Age Adult "adult"
expect Language 409 "US English"
shown=$(printf '%s\n' "$out" | grep -E '^(description|description409|Name|Vendor)=')
printf '%s\n' "$shown" | grep -qiE 'windows|microsoft|wine' && fail "a name the user sees says Windows, Microsoft or Wine" \
    || pass "no Windows, Microsoft or Wine in what the user sees"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
