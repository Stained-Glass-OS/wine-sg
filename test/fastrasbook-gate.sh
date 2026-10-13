#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# rasapi32 phone book (rasphone.pbk), dial parameters, credentials, sub-entries,
# automatic dialling and connection notifications (patch 3181), 64- and 32-bit,
# in a scratch prefix (the phone books are files inside it). Notifications run
# against a stand-in sg-netctl that lists a VPN connection while a state file
# exists; everything else with no sg-netctl at all.
#
#   WINE=/opt/wine-sg/bin/wine test/fastrasbook-gate.sh
# Mutants: SG_MUTANT_RAS_NOADMIN (rasapi32/phonebook.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-fastrasbook.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastrasbook-probe.c" -lrasapi32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
cat > "$T/netctl" <<'EOS'
#!/bin/sh
[ "$1" = vpn ] && [ "$2" = list ] || { echo "ERROR invalid unknown command"; exit 2; }
if [ -f "$FAKE_STATE" ]; then s=connected; else s=disconnected; fi
printf 'VPN 1111-aaaa\twireguard\t%s\tOffice VPN\n' "$s"
echo OK
EOS
chmod +x "$T/netctl"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

GUI=
run() {
    name=$1; netctl=$2; shift 2
    for a in x86_64 i686; do
        out=$(cd "$T" && SG_NETCTL="$netctl" FAKE_STATE="$T/state" timeout -s KILL 240 ${GUI:-env DISPLAY=} "$WINE" "$T/probe-$a.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "PASS  $name ($a)"
        else echo "FAIL  $name ($a)"; printf '%s\n' "$out" | grep -v '^PASS' | sed 's/^/        /'; RC=1; fi
        "$WINESERVER" -w
    done
}
rm -f "$T/state"
run "phone book entries, dial parameters, credentials, sub-entries, autodial" /nonexistent/sg-netctl book
run "a non-administrator may not change the all-users phone book" /nonexistent/sg-netctl nonadmin
if command -v xvfb-run >/dev/null; then
    GUI="xvfb-run -a"
    run "the new/edit phone book entry dialogs" /nonexistent/sg-netctl dialog
    GUI=
fi
run "connection notifications" "$T/netctl" notify "$T/state"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
