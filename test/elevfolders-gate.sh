#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An elevated program's Start menu and desktop are the ones all users share
# (patches/sg/0617). The elevation broker runs a program as the SYSTEM
# account, not as the user who asked; an installer's own-user shortcuts went
# to SYSTEM's Start menu, which nobody sees (Blender, Temurin, Audacity,
# Winamp: not in Start). In a program the broker started (SG_IN_BROKER) the
# Programs, Startup and desktop folders -- by CSIDL and by known folder -- are
# the all-users ones; its application data stays its own. Elsewhere, nothing
# changes.
#
#   WINE=/opt/wine-sg/bin/wine test/elevfolders-gate.sh
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
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-elevfolders.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/elevfolders-probe.exe" "$HERE/elevfolders-probe.c" -lshell32 -lole32 -luuid ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/elevfolders-probe.exe" "$WINEPREFIX/drive_c/"
P() { "$WINE" 'C:\elevfolders-probe.exe' 2>/dev/null | tr -d '\r'; }
v() { echo "$1" | sed -n "s/^$2=//p"; }
plain=$(P)
elev=$(SG_IN_BROKER=1 P)
common=$(v "$plain" common-programs)
echo "      plain: $(v "$plain" programs)"
echo "      elevated: $(v "$elev" programs)"
case "$(v "$plain" programs)" in *ProgramData*) fail "a plain program's Programs folder is all users': $(v "$plain" programs)" ;;
    *) pass "a plain program keeps its own Start menu" ;; esac
[ -n "$common" ] && [ "$(v "$elev" programs)" = "$common" ] && [ "$(v "$elev" known-programs)" = "$common" ] &&
    pass "elevated: Programs is all users' (CSIDL and known folder)" ||
    fail "elevated Programs: $(v "$elev" programs) / $(v "$elev" known-programs), all users': $common"
case "$(v "$elev" startup)" in *ProgramData*) pass "elevated: Startup is all users'" ;; *) fail "elevated Startup: $(v "$elev" startup)" ;; esac
[ "$(v "$elev" desktop)" != "$(v "$plain" desktop)" ] && case "$(v "$elev" desktop)" in *Public*|*ProgramData*) true ;; *) false ;; esac &&
    pass "elevated: the desktop is the shared one ($(v "$elev" desktop))" || fail "elevated desktop: $(v "$elev" desktop)"
[ "$(v "$elev" appdata)" = "$(v "$plain" appdata)" ] && pass "its application data stays its own" || fail "appdata moved: $(v "$elev" appdata)"
exit $RC
