#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI's default audio output token (6092). AudioOutput's DefaultDefaultTokenId names the MMAudioOut token
# enumerator's root (...\TokenEnums\MMAudioOut\), whose CLSID is the enumerator Wine does not have, so a token set
# to it made no ISpAudio (REGDB_E_CLASSNOTREG): ScummVM said "Could not initialize TTS audio" and had no speech.
# Now such ids are the enumerator's tokens -- the default device for the root, waveOut device N for "\N" -- with
# CLSID_SpMMAudioOut, the device's name and its DeviceId. 64-bit and 32-bit (ScummVM is 32-bit).
#   WINE=/opt/wine-sg/bin/wine test/sapiaudioout-gate.sh   (mutant SG_MUTANT_NO_MMAUDIO_TOKENS)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapiaudioout.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for arch in x86_64 i686; do
    command -v $arch-w64-mingw32-gcc >/dev/null || { echo "SKIP  $arch: no compiler"; continue; }
    TMPDIR=/var/tmp $arch-w64-mingw32-gcc -O1 -o "$T/probe-$arch.exe" "$HERE/sapiaudioout-probe.c" -lole32 -luuid 2>"$T/cc.log" ||
        { cat "$T/cc.log"; fail "$arch probe did not build"; continue; }
    out=$(timeout -s KILL 120 "$WINE" "$T/probe-$arch.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    expect() {
        if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$arch: $3"
        else fail "$arch: $3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
    }
    expect default_id "0 1" "AudioOutput's default token is the MMAudioOut enumerator's root"
    expect set_id 0 "a token can be set to it"
    expect clsid "0 1" "it is an SpMMAudioOut token"
    expect has_name 1 "with a name"
    expect create_audio "0 1" "and makes an ISpAudio"
    expect missing_device 0x8004503a "a device that is not there is SPERR_NOT_FOUND"
    expect done 1 "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
