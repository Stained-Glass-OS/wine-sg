#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Map Network Drive on a share that refuses the signed-in user
# (patches/sg/0839). It showed "The folder cannot be reached. Access denied."
# and stopped; as Windows' does, it now asks for a name and password, asks
# again ("Logon unsuccessful") while they are refused, and maps the drive
# with the right ones. The network side is a stand-in for sg-netmountd
# (test/mapcred-netmountd.py) on /run/stained-glass-net/netmount.sock (with
# sudo, removed after): every share refuses a connection until LOGON gives
# the password.
# Mutant: SG_MUTANT_MAP_NO_CRED_RETRY in dlls/mpr/mapdlg.c.
#
#   WINE=... test/mapcred-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
NET=/run/stained-glass-net
DRIVES=$NET/drives/$(id -u)
RC=0; XP=; DP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
[ -e "$NET" ] && { echo "SKIP: $NET exists (sg-netmountd's)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-mapcred.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "$DP" ] && kill "$DP" 2>/dev/null
    sudo -n rm -rf "$NET"
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/mapcred-probe.exe" "$HERE/mapcred-probe.c" -lmpr || { echo "FAIL  probe did not build"; exit 1; }
sudo -n mkdir -p "$DRIVES" && sudo -n chown -R "$(id -u)" "$NET" || { echo "SKIP: cannot make $NET"; exit 77; }
mkdir "$T/share"; echo hello > "$T/share/hello.txt"
python3 "$HERE/mapcred-netmountd.py" "$NET/netmount.sock" "$DRIVES" "$T/share" Right1pass "$T/requests" & DP=$!
i=0; while [ ! -S "$NET/netmount.sock" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done

unset DISPLAY XAUTHORITY
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
DPY=":$(cat "$T/display")"
case "$DPY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
export DISPLAY="$DPY"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/mapcred-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" 'C:\mapcred-probe.exe' '\\sgcredsrv\locked' Right1pass > "$T/out" 2>/dev/null </dev/null)
out=$(tr -d '\r' < "$T/out")
echo "      $out"
sed 's/^/      server: /' "$T/requests" 2>/dev/null
case "$out" in *'prompts=2 '*) pass "a refused share asks for a name and password, and again when they are wrong" ;;
    *) fail "the dialog asked $(printf '%s' "$out" | sed -n 's/.*prompts=\([0-9]*\).*/\1/p') times" ;; esac
case "$out" in *'errors=0 '*) pass "with no 'cannot be reached' box" ;; *) fail "it showed an error box" ;; esac
case "$out" in *'result=0 '*'mapped=1'*) pass "and maps the drive with the right password" ;; *) fail "not mapped: $out" ;; esac
grep -q '^LOGON sgcredsrv locked .*tester -> OK' "$T/requests" && pass "as the name typed, for that server" || fail "no LOGON with the typed name"
grep -q 'Right1pass\|wrong' "$T/requests" && fail "a password reached the log" || true

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
