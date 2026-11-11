#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A page an elevated program opens opens unelevated, as the person
# (patches/sg/7736; David 2026-11-11: a browser must not run with the system
# account's rights).
#
# An elevated program runs as the SYSTEM account on disk. FreeOffice's
# uninstaller opened its survey page and Firefox ran as that account on the
# person's desktop. Now shell32 hands the page to the person's own shell
# (their explorer.exe), which opens it as them; with no such shell it is not
# opened at all. As in elevid-gate.sh the prefix belongs to SYSTEM
# (SG_SYSTEM) and a standard user (SG_USER1) has a shell desktop in it; the
# gate's https handler records who it ran as:
#  1. a page the person's elevated program opens: the handler runs as the
#     person, NOT elevated
#  2. a page the person opens themselves: as before
#  3. the person's shell gone: the elevated program's page is not opened, and
#     ShellExecuteEx fails (access denied)
#  4. SYSTEM's own program (no person): not opened either
#  5. another account serving the person's pipe name first is not given the
#     page (shell32 checks the server is the person, unelevated)
#  6. another account cannot ask the person's shell to open a page (the
#     shell checks who asks; refused, SE_ERR_ACCESSDENIED)
# Exit 77 without the accounts or passwordless sudo.
# Mutants: SG_MUTANT_ELEVATED_BROWSER (dlls/shell32/shlexec.c),
#          SG_MUTANT_UNELEVATED_OPEN_ANYONE (programs/explorer/desktop.c).
#   WINE=... WINESERVER=... test/elevurl-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}
SG_USER1=${SG_USER1:-sgtest1}
SG_USER2=${SG_USER2:-sgtest2}
SG_GROUP=${SG_GROUP:-sgwine}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/elevurl.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
LOG=$W/log/opened.txt
GLOG="Z:$(printf '%s' "$LOG" | tr / '\\')"
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=${GATE_DEBUG:--all} \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" SG_GATE_LOG="$GLOG" DISPLAY= "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -municode -O1 -o "$W/elevurl-probe.exe" "$HERE/elevurl-probe.c" -ladvapi32 -lshell32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/elevurl-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
install -d -m 777 "$W/log"; install -m 666 /dev/null "$LOG"
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/elevurl-probe.exe" "$PFX/drive_c/elevurl-probe.exe"
as "$SG_SYSTEM" timeout -s KILL 120 "$WINE" reg add 'HKLM\Software\Classes\https\shell\open\command' /ve /d '"C:\elevurl-probe.exe" handle "%1"' /f >/dev/null 2>&1
as "$SG_USER1" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
uid1=$(id -u "$SG_USER1")
elevated() { as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 USER="$SG_USER1" timeout -s KILL 90 "$WINE" "$@" 2>>"$W/elevated.err" | tr -d '\r'; }
opened() {   # opened URL: the handler's line for URL (waits a little)
    local i
    for i in $(seq 1 20); do
        tr -d "\r" 2>/dev/null <"$LOG" | grep -F "$1 " | head -1 | grep . && return
        sleep 0.5
    done
}
U=https://example.test/sg-elevurl
out=$(as "$SG_USER1" timeout -s KILL 90 "$WINE" 'C:\elevurl-probe.exe' open "$U/2" 2>/dev/null | tr -d '\r')
got=$(opened "$U/2")
sid1=$(printf '%s' "$got" | sed -n 's/.* elevated 0 \(S-1-5-21-[0-9-]*\)$/\1/p')
[ -n "$sid1" ] && pass "a page $SG_USER1 opens: as before, as $sid1" || fail "user's own page: '$got' ($out)"

# another account serving the person's pipe name before their shell does
as "$SG_USER2" sh -c "exec timeout -s KILL 70 '$WINE' 'C:\\elevurl-probe.exe' squat '$sid1'" >"$W/squat.out" 2>/dev/null &
SQ=$!
for i in $(seq 1 60); do grep -q SQUAT "$W/squat.out" 2>/dev/null && break; sleep 0.5; done
out=$(elevated 'C:\elevurl-probe.exe' open "$U/5")
got=$(opened "$U/5"); sq=$(opened "squatter $U/5")
[ "$(tr -d '\r' <"$W/squat.out")" = "SQUAT ready" ] && [ -z "$got$sq" ] && [ "$out" = "OPEN fail 5" ] \
    && pass "$SG_USER2 serving $SG_USER1's pipe first is not given the page, nor is it opened" \
    || fail "squatter: $(tr -d '\r' <"$W/squat.out"), squatter got '$sq', handler '$got', $out"
kill $SQ 2>/dev/null; sudo -n pkill -KILL -u "$SG_USER2" -f elevurl-probe 2>/dev/null; wait $SQ 2>/dev/null; sleep 1

# the person's shell: a shell desktop of theirs (explorer.exe, null display)
as "$SG_USER1" sh -c "exec timeout -s KILL 600 '$WINE' explorer /desktop=shell,800x600" >"$W/explorer.log" 2>&1 &
EXPL=$!
shell=$(as "$SG_USER1" timeout -s KILL 90 "$WINE" 'C:\elevurl-probe.exe' shell 2>/dev/null | tr -d '\r')
[ "$shell" = "SHELL yes" ] || { fail "$SG_USER1's shell does not serve elevated programs' pages ($shell)"; }

out=$(elevated 'C:\elevurl-probe.exe' open "$U/1")
got=$(opened "$U/1")
echo "      elevated: $out / handler: $got"
[ "$got" = "$U/1 elevated 0 $sid1" ] && pass "a page the elevated program opens opens as $SG_USER1, unelevated" \
    || fail "elevated program's page: '$got' ($out)"
[ "$out" = "OPEN ok 0" ] && pass "...and ShellExecuteEx says it was opened" || fail "elevated ShellExecuteEx: $out"


out=$(as "$SG_USER2" timeout -s KILL 90 "$WINE" 'C:\elevurl-probe.exe' send "$sid1" "$U/6" 2>/dev/null | tr -d '\r')
got=$(opened "$U/6")
[ -z "$got" ] && case "$out" in "SEND fail"*|"SEND ok 5") true ;; *) false ;; esac \
    && pass "$SG_USER2 cannot ask $SG_USER1's shell to open a page ($out)" || fail "other account's request: $out, handler '$got'"

for p in $(pgrep -u "$SG_USER1" -f 'explorer'); do   # this gate's explorer only
    sudo -n grep -qzx "WINEPREFIX=$PFX" "/proc/$p/environ" 2>/dev/null && sudo -n kill -KILL "$p"
done
wait $EXPL 2>/dev/null; sleep 2
out=$(elevated 'C:\elevurl-probe.exe' open "$U/3")
got=$(opened "$U/3")
[ -z "$got" ] && [ "$out" = "OPEN fail 5" ] && pass "with no shell of $SG_USER1's the page is not opened (access denied), not opened elevated" \
    || fail "no shell: handler '$got', $out"

out=$(as "$SG_SYSTEM" timeout -s KILL 90 "$WINE" 'C:\elevurl-probe.exe' open "$U/4" 2>/dev/null | tr -d '\r')
got=$(opened "$U/4")
[ -z "$got" ] && pass "SYSTEM's own program's page is not opened either ($out)" || fail "SYSTEM: handler '$got', $out"

as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
