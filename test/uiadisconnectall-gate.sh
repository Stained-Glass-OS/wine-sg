#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UiaDisconnectAllProviders (patches/sg/7000): Electron and Chromium (Element, Discord, Slack, VS Code)
# delay-import it from uiautomationcore, which did not export it, so their delay-load helper failed
# where they call it. Now it disconnects every provider node the process has handed to clients: the
# providers are released, and a client in another process that still holds the element gets
# UIA_E_ELEMENTNOTAVAILABLE, as on Windows. The probe (test/uiadisconnectall-probe.c) is a
# provider window plus a client child process, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/uiadisconnectall-gate.sh
# Mutant: SG_MUTANT_UIA_DISCONNECT_ALL_KEEPS (dlls/uiautomationcore/uia_provider.c) disconnects nothing.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-uiadisc.XXXXXX)
XPID=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XPID" ] && kill $XPID 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/uiadisconnectall-probe.c" -lole32 -loleaut32 -luiautomationcore -luser32 -luuid \
        || { fail "probe ($b-bit) did not build"; exit 1; }
done
D=${SG_GATE_DISPLAY:-:287}
Xvfb "$D" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XPID=$!
export DISPLAY="$D" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
sleep 1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check exported                 "uiautomationcore exports UiaDisconnectAllProviders"
    check empty_ok                 "with no provider handed out it returns S_OK"
    check client_name_before       "a client in another process reads the provider's name"
    check disconnect_ok            "UiaDisconnectAllProviders returns S_OK with a client connected"
    check provider_released        "the provider's references held for the client are released"
    check client_unavailable_after "the client's element is then UIA_E_ELEMENTNOTAVAILABLE"
    check client_release           "the client can still release its node"
    check done                     "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
