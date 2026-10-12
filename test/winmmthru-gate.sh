#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# midiConnect/midiDisconnect and MCI_OPEN element+id (patches 2976-2977): argument errors, connect/disconnect
# state machine, drop on close; test/winmmthru-probe.c. Parts needing a MIDI
# device are skipped and say so. Stderr is checked for the old FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/winmmthru-gate.sh
# Mutants (winmm, -DSG_MUTANT_x): MIDI_DISCONNECT, MIDI_DROP_ON_CLOSE, MIDI_RESERVED, MIDI_THRU_FLAGS, MCI_ELEMENT_ID.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winmmthru.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/winmmthru-probe.exe" "$HERE/winmmthru-probe.c" -lwinmm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/winmmthru-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" winmmthru-probe.exe 2> "$T/probe.err" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:.*midi(Connect|Disconnect)' "$T/probe.err"; then echo "FAIL  midiConnect/midiDisconnect still log a FIXME"; exit 1; fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
