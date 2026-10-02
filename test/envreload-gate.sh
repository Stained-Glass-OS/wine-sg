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
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/envprobe.exe" "$HERE/envreload-probe.c" -lntdll || { fail "probe did not build"; exit 1; }
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
p del SG_ENV_GATE >/dev/null; sleep 1
[ "$(p get SG_ENV_GATE)" = "SG_ENV_GATE=(none)" ] && pass "and one removed goes" || fail "after removing: $(p get SG_ENV_GATE)"
[ "$(p get SG_SESSION_ONLY)" = "SG_SESSION_ONLY=kept" ] && pass "the session's own variables stay" || fail "session variable: $(p get SG_SESSION_ONLY)"
exit $RC
