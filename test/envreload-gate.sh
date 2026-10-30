#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell reads environment variables again when told they changed
# (patches/sg/0749), as Windows' does: WM_SETTINGCHANGE "Environment" (the
# Environment Variables dialog, setx, installers) -- so programs started after
# a change get it without signing in again (David 2026-10-01: Claude Code asked
# for PATH to be changed). Only what the registry defines changes: the
# session's own variables stay.
#
#   WINE=/opt/wine-sg/bin/wine test/envreload-gate.sh
# Mutation: -DSG_MUTANT_NO_ENV_RELOAD (the shell keeps its old environment).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-envreload.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${SG_KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/envprobe.exe" "$HERE/envreload-probe.c" -luserenv -ladvapi32 -lntdll || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/envprobe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
SG_SESSION_ONLY=kept "$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 8
p() { (cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" envprobe.exe "$@" 2>/dev/null | tr -d '\r'); }
[ "$(p get SG_SESSION_ONLY)" = "SG_SESSION_ONLY=kept" ] || fail "the shell did not start with the session variable: $(p get SG_SESSION_ONLY)"
p set SG_ENV_GATE added >/dev/null; sleep 1
[ "$(p get SG_ENV_GATE)" = "SG_ENV_GATE=added" ] && pass "a variable added to the user's environment reaches the shell" \
    || fail "after adding: $(p get SG_ENV_GATE)"
p set Path 'C:\sg-gate-bin' >/dev/null; sleep 1
case "$(p get PATH)" in *';C:\sg-gate-bin') pass "the user's Path is added after the system's ($(p get PATH | cut -c1-60)...)";;
    *) fail "PATH: $(p get PATH)";; esac
# an expandable value naming another variable (the Default User profile's
# TEMP=%USERPROFILE%\AppData\Local\Temp) reaches programs expanded: the shell
# handed out the text itself, and Prism Launcher -- started from Start, the
# desktop or Run -- exited at once (qa4, release 621)
p setx TEMP '%USERPROFILE%\AppData\Local\Temp' >/dev/null; sleep 1
case "$(p get TEMP)" in *%*) fail "TEMP reached programs unexpanded: $(p get TEMP)";;
    'TEMP=C:\users\'*'\AppData\Local\Temp') pass "an expandable TEMP reaches programs expanded ($(p get TEMP))";;
    *) fail "TEMP: $(p get TEMP)";; esac
# CreateEnvironmentBlock for an account with no ProfileList entry and no
# Volatile Environment (a shared prefix's users have neither): it gave no USERNAME or USERPROFILE -- its account
# lookup had no buffer for the domain -- so TEMP stayed "%USERPROFILE%\..."
# and the shell handed that out after a broadcast (qa4: Prism Launcher from
# Start exited at once). Mutant SG_MUTANT_CEB_NO_DOMAIN.
sid=$(p ceb >/dev/null; "$WINE" reg query 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\ProfileList' 2>/dev/null | tr -d '\r' | grep -o 'S-1-5-21-[0-9-]*' | head -1)
"$WINE" reg delete 'HKCU\Volatile Environment' /f >/dev/null 2>&1   # a shared prefix's users have none either
[ -n "$sid" ] && "$WINE" reg delete "HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\$sid" /f >/dev/null 2>&1
out=$(p ceb | tr '\n' ' ')
case "$out" in *"USERNAME=(none)"*|*"USERPROFILE=(none)"*|*%*) fail "with no ProfileList entry the block lacks the user: $out";;
    *"USERPROFILE=C:"*"TEMP=C:"*) pass "with no ProfileList entry the block has USERNAME, USERPROFILE and an expanded TEMP";;
    *) fail "block: $out";; esac
p set SG_ENV_GATE2 x >/dev/null; sleep 1
case "$(p get TEMP)" in *%*) fail "after a broadcast the shell hands out TEMP unexpanded: $(p get TEMP)";;
    'TEMP=C:\users\'*) pass "and after a broadcast the shell hands programs a real TEMP";;
    *) fail "TEMP after broadcast: $(p get TEMP)";; esac
p del SG_ENV_GATE >/dev/null; sleep 1
[ "$(p get SG_ENV_GATE)" = "SG_ENV_GATE=(none)" ] && pass "and one removed goes" || fail "after removing: $(p get SG_ENV_GATE)"
[ "$(p get SG_SESSION_ONLY)" = "SG_SESSION_ONLY=kept" ] && pass "the session's own variables stay" || fail "session variable: $(p get SG_SESSION_ONLY)"
exit $RC
