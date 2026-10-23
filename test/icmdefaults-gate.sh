#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WCS default color profiles and rendering intents (mscms) and the color management dialog
# (icmui SetupColorMatchingA/W), patches/sg/6506 -- the defaults were ERROR_CALL_NOT_IMPLEMENTED,
# the setters, associations and enumeration stubs, the dialog failed. test/icmdefaults-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/icmdefaults-gate.sh
# Mutants: SG_MUTANT_WCS_DEFAULT_IGNORES_LIST (mscms profile.c), SG_MUTANT_ICMUI_NOT_STORED (icmui.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icmdefaults.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
# the probe is linked against Wine's own import libraries' names (mingw lacks the WCS ones): by name at run time
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/icmdefaults-probe.c" -lmscms -licmui -luser32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check intent_default          "the default rendering intent is perceptual"
check intent_user_from_system "the user's is the system's until set"
check intent_user             "the user's own intent"
check intent_system           "the system's stays"
check intent_bad              "an unknown intent is ERROR_INVALID_PARAMETER"
check installed               "a profile of the probe's own is installed"
check enum                    "WcsEnumColorProfiles lists the installed profiles"
check enum_own                "the new one among them"
check default_srgb            "a device with no association: the sRGB profile, and its size"
check default_small           "too small a buffer: ERROR_INSUFFICIENT_BUFFER"
check associated              "an associated profile is the device's default"
check associate_missing       "a profile not installed: ERROR_FILE_NOT_FOUND"
check per_user_off            "the user's list does not count without per-user profiles"
check per_user_on             "and does with them"
check disassociated           "disassociated: back to sRGB"
check disassociate_again      "disassociating it again: ERROR_FILE_NOT_FOUND"
check dialog_invalid          "SetupColorMatchingW of an empty structure: ERROR_INVALID_PARAMETER"
check dialog_monitor_list     "the dialog lists the profiles"
check dialog_ok               "the dialog's choices are written back, Apply calls the callback"
check dialog_overflow         "a profile buffer too small: CMS_MONITOROVERFLOW, ERROR_INSUFFICIENT_BUFFER"
check done                    "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
