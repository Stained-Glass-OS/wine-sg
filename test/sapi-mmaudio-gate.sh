#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI's wave output device SpMMAudioOut (patches/sg/2843), on Xvfb:
# test/sapi-mmaudio-probe.c checks the event source and sink (interest, queue,
# copies of string/object/pointer parameters, the notification kinds), the
# IStream members of an output device, GetStatus, buffer info, default format,
# volume, MM handle, the pause and stop states (when the machine has a wave
# device; the rest is skipped otherwise) and the device id from an object
# token. Before the patch these were FIXME stubs returning E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/sapi-mmaudio-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (sapi, -DSG_MUTANT_x): SAPIMM_SEEK_POS (Seek reports no position),
# SAPIMM_STATUS_SEEKPOS, SAPIMM_BUFINFO_ANY (zero buffer info accepted),
# SAPIMM_STATE_ANY (invalid states accepted), SAPIMM_TOKEN_DEVID (the token's
# DeviceId ignored), SAPIMM_SINK_DROP (AddEvents drops everything),
# SAPIMM_READ_OK (Read succeeds), SAPIMM_VOLUME_RANGE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapimm.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sapi-mmaudio-probe.exe" "$HERE/sapi-mmaudio-probe.c" -lsapi -luuid -lole32 -loleaut32 -lwinmm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sapi-mmaudio-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" sapi-mmaudio-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
