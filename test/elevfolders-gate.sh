#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Where an elevated program's Start menu and desktop are (patches/sg/0617, 9004).
# The elevation broker runs a program on disk as the SYSTEM account. Before
# wine-sg 7601 it had SYSTEM's token too, and an installer's own-user shortcuts
# went to SYSTEM's Start menu, which nobody sees (Blender, Temurin, Audacity,
# Winamp): 0617 gave such a program the all-users Programs, Startup and desktop.
# Since 7601 an elevated program is the person who asked (their SID, HKCU and
# profile), and on Windows an elevated per-user install keeps that person's
# Start menu and desktop -- Mixxx's per-user MSI put its shortcuts in all
# users' Start menu. So (9004):
#   - a program the broker started (SG_IN_BROKER) that is the person keeps the
#     person's Programs, Startup and desktop (CSIDL and known folder);
#   - one that really is the SYSTEM account (the owner of a shared prefix,
#     SG_GROUP) gets the all-users ones, as before; its application data stays
#     its own. Mutant SG_MUTANT_ELEVATED_ALWAYS_COMMON (dlls/shell32).
# And a user's own programs folder, FOLDERID_UserProgramFiles, exists (0618).
#
#   WINE=/opt/wine-sg/bin/wine test/elevfolders-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
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
echo "      plain: $(v "$plain" programs)"
echo "      elevated, the person: $(v "$elev" programs)"
case "$(v "$plain" programs)" in *ProgramData*) fail "a plain program's Programs folder is all users': $(v "$plain" programs)" ;;
    *) pass "a plain program keeps its own Start menu" ;; esac
[ "$(v "$elev" programs)" = "$(v "$plain" programs)" ] && [ "$(v "$elev" known-programs)" = "$(v "$plain" programs)" ] &&
    [ "$(v "$elev" startup)" = "$(v "$plain" startup)" ] && [ "$(v "$elev" desktop)" = "$(v "$plain" desktop)" ] &&
    pass "elevated, the person: their own Programs, Startup and desktop (CSIDL and known folder)" ||
    fail "elevated person: programs $(v "$elev" programs) startup $(v "$elev" startup) desktop $(v "$elev" desktop)"
# the SYSTEM account: the owner of a shared prefix
SG_GROUP=${SG_GROUP:-sgconfgrp}
if getent group "$SG_GROUP" | grep -qw "$(id -un)"; then
    SP="$T/shared"
    mkdir "$SP"; chgrp "$SG_GROUP" "$SP"; chmod 2770 "$SP"; touch "$SP/.sg-system-prefix"
    S() { sg "$SG_GROUP" -c "umask 002; WINEPREFIX='$SP' $1 timeout -s KILL 60 '$WINE' 'C:\\elevfolders-probe.exe'" 2>/dev/null | tr -d '\r'; }
    sg "$SG_GROUP" -c "umask 002; WINEPREFIX='$SP' '$WINESERVER' -p; WINEPREFIX='$SP' timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
    cp "$T/elevfolders-probe.exe" "$SP/drive_c/"
    splain=$(S "")
    sys=$(S "SG_IN_BROKER=1")
    common=$(v "$splain" common-programs)
    echo "      elevated, SYSTEM: $(v "$sys" programs)"
    [ -n "$common" ] && [ "$(v "$sys" programs)" = "$common" ] && [ "$(v "$sys" known-programs)" = "$common" ] &&
        pass "elevated, the SYSTEM account: Programs is all users' (CSIDL and known folder)" ||
        fail "SYSTEM Programs: $(v "$sys" programs) / $(v "$sys" known-programs), all users': $common"
    case "$(v "$sys" startup)" in *ProgramData*) pass "...Startup is all users'" ;; *) fail "SYSTEM Startup: $(v "$sys" startup)" ;; esac
    case "$(v "$sys" desktop)" in *Public*|*ProgramData*) pass "...the desktop is the shared one ($(v "$sys" desktop))" ;;
        *) fail "SYSTEM desktop: $(v "$sys" desktop)" ;; esac
    [ "$(v "$sys" appdata)" = "$(v "$splain" appdata)" ] && pass "...its application data stays its own" || fail "appdata moved: $(v "$sys" appdata)"
    WINEPREFIX="$SP" "$WINESERVER" -k 2>/dev/null
else
    echo "SKIP  the SYSTEM account's part: $(id -un) is not in $SG_GROUP"
fi
# a user's own programs folder (0618): %LOCALAPPDATA%\Programs -- it was
# "disallowed" and Opera's installer stopped with code 103
lad=$(v "$plain" localappdata)
[ -n "$lad" ] && [ "$(v "$plain" user-program-files)" = "$lad\Programs" ] &&
    [ "$(v "$plain" user-program-files-common)" = "$lad\Programs\Common" ] &&
    pass "FOLDERID_UserProgramFiles is $(v "$plain" user-program-files) (and its Common)" ||
    fail "FOLDERID_UserProgramFiles: '$(v "$plain" user-program-files)' common '$(v "$plain" user-program-files-common)' (local app data $lad)"
exit $RC
