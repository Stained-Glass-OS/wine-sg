#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dpnet's FIXME stubs (patches/sg/2980), on Xvfb: test/dpnet-session-probe.c
# drives the DirectPlay8 Peer / Client / Server / LobbiedApplication /
# LobbyClient / ThreadPool objects through their post-Initialize states
# (host, connect with no answer, application description, local player info,
# groups, contexts, async handles and cancellation, loopback sends, caps,
# thread counts, registered lobby programs) and checks every HRESULT and the
# messages delivered to the handler. The log is also checked: none of the
# formerly stubbed methods may log a FIXME any more.
#
#   WINE=/opt/wine-sg/bin/wine test/dpnet-session-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dpnet, -DSG_MUTANT_x): UNINIT (no UNINITIALIZED answers), APPDESC
# (SetApplicationDesc ignores the player limit), GROUPS (a player can join a
# group twice), ASYNC (a cancelled operation completes with S_OK), CAPS
# (SetCaps drops the retries), CONNECT (a sync connect succeeds), SEND (loopback
# ignores NOLOOPBACK), INFO (info is returned without its name), BUFFER (wrong
# size on BUFFERTOOSMALL), THREADS (DoWork ignores running threads), LOBBY (a
# registered program is not listed), FIXME (Close logs a FIXME again).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dpnetsession.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dpnet WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/dpnet-session-probe.exe" "$HERE/dpnet-session-probe.c" -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dpnet-session-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" dpnet-session-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed methods may log a FIXME
if grep -E 'fixme:dpnet:(IDirectPlay8(Peer|Client|Server|LobbiedApplication|ThreadPool)Impl_(Cancel|Connect|Send|GetSendQ|Host|GetApp|SetApp|Create|Destroy|AddPlayer|RemovePlayer|SetGroup|GetGroup|EnumPlayers|EnumGroup|SetPeer|GetPeer|GetLocal|Close|ReturnBuffer|GetPlayerC|GetGroupC|GetCaps|SetCaps|GetConn|RegisterLobby|Terminate|SetServer|SetClient|GetServer|GetClient|UnRegister|Register|SetAppAv|UpdateS|GetThreadC|SetThreadC|DoWork)|lobbyclient_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed method logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed methods logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
