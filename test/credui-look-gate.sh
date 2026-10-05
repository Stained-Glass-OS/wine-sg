#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The network credentials prompt (CredUIPromptForCredentials: mapped drives,
# proxies, installers that ask for a share's password) is drawn as since
# Vista (patches/sg/0809): a heading in a larger face, "Enter your
# credentials to connect to: <target>", the user name and password fields
# labelled by cue banners (comctl32 v6 through the DLL's own manifest), on
# the window colour -- not Windows 2000's banner bitmap and grey form.
#
#   WINE=/opt/wine-sg/bin/wine test/credui-look-gate.sh   (mutants SG_MUTANT_CREDUI_V5, SG_MUTANT_CREDUI_GREY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-credui-look.XXXXXX)
n=180; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=":$n"
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/credui-look-probe.c" -lcredui -lcomctl32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
val() { sed -n "s/^$1=//p" "$T/o" | head -1; }
[ "$(val TITLE)" = "Connect to fileserver" ] && pass "the title names the target" || fail "title '$(val TITLE)'"
[ "$(val HEADER)" = "Enter network credentials" ] && pass "a heading: Enter network credentials" || fail "heading '$(val HEADER)'"
[ "$(val MESSAGE)" = "Enter your credentials to connect to: fileserver" ] && pass "the message names the target" || fail "message '$(val MESSAGE)'"
[ "$(val HEADERBIGGER)" = 1 ] && pass "the heading is in a larger face than the form" || fail "heading face not larger"
[ "$(val CUEUSER)" = "User name" ] && pass "the user name field shows 'User name' while empty" || fail "user name cue '$(val CUEUSER)'"
[ "$(val CUEPASS)" = "Password" ] && pass "the password field shows 'Password' while empty" || fail "password cue '$(val CUEPASS)'"
[ "$(val BACKGROUND)" = window ] && pass "the form is on the window colour" || fail "background '$(val BACKGROUND)'"
[ "$(val RET)" = 1223 ] && pass "Cancel returns ERROR_CANCELLED" || fail "cancel returned '$(val RET)'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; cat "$T/o"; }
exit "$RC"
