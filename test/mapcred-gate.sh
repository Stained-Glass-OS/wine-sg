#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Map Network Drive on a share that refuses the signed-in user
# (patches/sg/0839). It showed "The folder cannot be reached. Access denied."
# and stopped; as Windows' does, it now asks for a name and password, asks
# again ("Logon unsuccessful") while they are refused, and maps the drive
# with the right ones (mutant SG_MUTANT_MAP_NO_CRED_RETRY in
# dlls/mpr/mapdlg.c).
#
# "Reconnect at sign-in" (also patches/sg/0839): the drive is remembered and, with
# "Remember my credentials", the name and password for its server (mutant
# SG_MUTANT_NO_SAVED_CREDS in dlls/mpr/mapdlg.c); after a sign-out (the
# drive and the connection gone) WNetRestoreConnection connects it again with
# them (mutant SG_MUTANT_NO_RESTORE in dlls/mpr/wnet.c), and so does the
# shell when a sign-in starts it (mutant SG_MUTANT_NO_SIGNIN_RESTORE in
# programs/explorer/startup.c); NET USE /PERSISTENT:YES /SAVECRED remembers
# the same way (SG_MUTANT_NO_SAVED_CREDS in programs/net/net.c).
#
# The network side is a stand-in for sg-netmountd (test/mapcred-netmountd.py)
# on /run/stained-glass-net/netmount.sock (with sudo, removed after): every
# share refuses a connection until LOGON gives the password.
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
for t in Xvfb xvfb-run "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
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
"$MINGW" -O2 -municode -o "$T/mapcred-probe.exe" "$HERE/mapcred-probe.c" -lmpr -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
sudo -n mkdir -p "$DRIVES" && sudo -n chown -R "$(id -u)" "$NET" || { echo "SKIP: cannot make $NET"; exit 77; }
mkdir "$T/share"; echo hello > "$T/share/hello.txt"
server() {   # server LOG: a stand-in sg-netmountd that knows no connection yet
    [ -n "$DP" ] && kill "$DP" 2>/dev/null && wait "$DP" 2>/dev/null
    rm -f "$NET/netmount.sock"
    python3 "$HERE/mapcred-netmountd.py" "$NET/netmount.sock" "$DRIVES" "$T/share" Right1pass "$1" & DP=$!
    i=0; while [ ! -S "$NET/netmount.sock" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
}
server "$T/requests"

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
printf '      %s\n' "$out"
sed 's/^/      server: /' "$T/requests" 2>/dev/null
case "$out" in *'prompts=2 '*) pass "a refused share asks for a name and password, and again when they are wrong" ;;
    *) fail "the dialog asked $(printf '%s' "$out" | sed -n 's/.*prompts=\([0-9]*\).*/\1/p') times" ;; esac
case "$out" in *'errors=0 '*) pass "with no 'cannot be reached' box" ;; *) fail "it showed an error box" ;; esac
case "$out" in *'result=0 '*'mapped=1'*) pass "and maps the drive with the right password" ;; *) fail "not mapped: $out" ;; esac
grep -q '^LOGON sgcredsrv locked .*tester -> OK' "$T/requests" && pass "as the name typed, for that server" || fail "no LOGON with the typed name"

# what is remembered for the next sign-in
case "$out" in *'remembered=\\sgcredsrv\locked'*) pass "the drive is remembered for the next sign-in (HKCU\\Network)" ;;
    *) fail "not remembered: $(printf '%s\n' "$out" | grep remembered)" ;; esac
case "$out" in *'saved=sgcredsrv\tester'*) pass "with the name and password, saved for the server" ;;
    *) fail "credentials: $(printf '%s\n' "$out" | grep saved)" ;; esac

# a sign-out: the drive and the connection go; then the drives come back
letter=$(printf '%s\n' "$out" | sed -n 's/^letter=\([A-Z]\):.*/\1/p' | tr 'A-Z' 'a-z')
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
rm -f "$DRIVES/$letter:"; server "$T/requests2"
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" rundll32.exe mpr.dll,SgRestoreConnections >/dev/null 2>&1 </dev/null)
sed 's/^/      server: /' "$T/requests2" 2>/dev/null
[ -n "$letter" ] && [ -L "$DRIVES/$letter:" ] && grep -q '^LOGON sgcredsrv locked sgcredsrv.tester -> OK' "$T/requests2" &&
    pass "WNetRestoreConnection connects the remembered drive again, with the saved credentials" ||
    fail "the remembered drive was not connected again"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w

rm -f "$DRIVES/$letter:"; server "$T/requests3"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/signin.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
XDG_SESSION_ID=4242 "$WINE" explorer /desktop=shell,800x600 > /dev/null 2>&1 &
i=0; while [ \$i -lt 40 ] && [ ! -L "$DRIVES/$letter:" ]; do sleep 1; i=\$((i + 1)); done
"$WINESERVER" -k
EOF2
chmod +x "$T/signin.sh"
unset DISPLAY
timeout -s KILL 200 xvfb-run -a -s "-screen 0 800x600x24" "$T/signin.sh" > /dev/null 2>&1
[ -L "$DRIVES/$letter:" ] && pass "and the shell does it at sign-in" || fail "the shell did not reconnect the drive at sign-in"

# NET USE /PERSISTENT:YES /SAVECRED
rm -f "$DRIVES"/*; server "$T/requests4"
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" net use 'X:' '\\sgnetuse\files' Right1pass /user:other /persistent:yes /savecred > "$T/out" 2>&1 </dev/null)
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" reg query 'HKCU\Network\X' /v RemotePath > "$T/reg" 2>&1 </dev/null)
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" 'C:\mapcred-probe.exe' saved sgnetuse > "$T/saved" 2>/dev/null </dev/null)
grep -q 'sgnetuse.files' "$T/reg" && grep -q 'saved=sgnetuse.other' "$T/saved" &&
    pass "NET USE /PERSISTENT:YES /SAVECRED remembers the drive and its credentials" ||
    fail "net use /persistent:yes /savecred: $(tr -d '\r' < "$T/reg" | grep -i remote) $(tr -d '\r' < "$T/saved")"

for f in "$T"/requests*; do grep -q 'Right1pass\|wrong' "$f" && fail "a password reached the log"; done

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
