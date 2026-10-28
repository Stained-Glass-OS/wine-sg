#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# crypt32's root import survives its importer's death (patches/sg/7600).
#
# One process imports the host's roots into the machine's Root store, the
# others wait for it. Its guard was a semaphore made at count 0: a process
# killed or crashed while importing never released it, and the processes
# waiting kept it open at 0 -- from then on every program's HTTPS hung in
# CertGetCertificateChain (SG Store: "Installing..." for good, qa4 walk).
# The probe takes the guards as an importer does, leaves them to a child that
# keeps them open, and ends; a new program must still read the roots.
#
#   WINE=/opt/wine-sg/bin/wine test/rootguard-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ls /etc/ssl/certs/*.pem >/dev/null 2>&1 || { echo "SKIP: no host CA certificates"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rootguard.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/rootguard-probe.c" -lcrypt32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINESERVER" -p
fail=0
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" use 2>/dev/null | tr -d '\r')
case "$out" in "roots="[1-9]*) echo "PASS  a program reads the machine's roots ($out)" ;;
  *) echo "FAIL  no roots for a program: '$out'"; fail=1 ;; esac
# (in the background, its output to a file: its keeper holds the output open, and a timeout
# would end the keeper with it)
"$WINE" "$T/probe.exe" dead > "$T/dead.txt" 2>/dev/null &
i=0; while ! grep -q keeper "$T/dead.txt" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
dead=$(tr -d '\r' < "$T/dead.txt")
printf '%s\n' "$dead" | sed 's/^/      /'
case "$dead" in *"importer ends holding"*) ;; *) echo "FAIL  the importer stand-in did not run"; echo "RESULT: FAIL"; exit 1 ;; esac
out=$(timeout -s KILL 30 "$WINE" "$T/probe.exe" use 2>/dev/null | tr -d '\r')
case "$out" in "roots="[1-9]*) echo "PASS  ...and still after the importer died holding its guard ($out)" ;;
  "") echo "FAIL  after the importer died, a program's root import hangs (30 s)"; fail=1 ;;
  *) echo "FAIL  after the importer died: '$out'"; fail=1 ;; esac
out=$(timeout -s KILL 30 "$WINE" "$T/probe.exe" use 2>/dev/null | tr -d '\r')
case "$out" in "roots="[1-9]*) echo "PASS  ...and the next program too ($out)" ;;
  *) echo "FAIL  the next program: '$out'"; fail=1 ;; esac
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
