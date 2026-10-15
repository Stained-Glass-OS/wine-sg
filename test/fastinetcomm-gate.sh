#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# inetcomm batch: stream/options/transmit info, address table, schema, security, message tree, offset tables, range lists, IMAP/SMTP/POP3 against a scripted server, mhtml protocol.
#
#   WINE=/opt/wine-sg/bin/wine test/fastinetcomm-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastinetcomm.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastinetcomm-probe.exe" "$HERE/fastinetcomm-probe.c" -idirafter "${WINE_INCLUDE:-/opt/wine-sg/include/wine/windows}" -lole32 -loleaut32 -luuid -lurlmon -lcrypt32 -lws2_32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastinetcomm-probe.exe" "$WINEPREFIX/drive_c/"
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
python3 "$HERE/fastinetcomm-server.py" "$T/ports" "$T/smtp.log" 2>"$T/server.err" &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; cleanup' EXIT INT TERM
i=0; while [ ! -s "$T/ports" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
cp "$T/ports" "$WINEPREFIX/drive_c/fastinetcomm-ports.txt"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastinetcomm-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null
rc=1
grep -q '^RESULT: PASS' "$T/probe.out" && rc=0
# what the SMTP transport put on the wire
if ! grep -aq 'RCPT TO: <rcpt@example.org> NOTIFY=SUCCESS,FAILURE' "$T/smtp.log"; then echo "FAIL  RCPT2 did not send the NOTIFY parameter"; rc=1; fi
if ! python3 -c "import sys; d=open(sys.argv[1],'rb').read(); sys.exit(0 if b'line one\r\n..dot line\r\n...two\r\nlast\r\n.\r\n' in d else 1)" "$T/smtp.log"; then echo "FAIL  data stream was not dot-stuffed / terminated"; rc=1; fi
[ -s "$T/server.err" ] && { echo "server errors:"; cat "$T/server.err"; rc=1; }
exit $rc
