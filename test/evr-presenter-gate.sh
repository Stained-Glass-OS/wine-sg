#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# evr's default presenter (patches/sg/2891), on Xvfb: test/evr-presenter-probe.c uses
# IMFVideoDisplayControl (border colour, rendering preferences, ideal size, repaint),
# IMFRateSupport, IQualProp (frames streamed through a mixer and a host), the quality
# interfaces, clock rate, ProcessMessage flush/step, the position mapper and
# IMFGetService of MFCreateVideoPresenter's presenter, also after shutdown.
#
#   WINE=/opt/wine-sg/bin/wine test/evr-presenter-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (evr, -DSG_MUTANT_x): BORDER_NOSTORE, PREFS_NOCHECK, RATE_NOCAP, STATS_NORESET, MAPPER_NOSRC,
# FLUSH_FAIL, IDEAL_ZERO, STATS_NOCOUNT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrpresenter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/evr-presenter-probe.exe" "$HERE/evr-presenter-probe.c" -I"$HERE" -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -ldxguid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/evr-presenter-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" evr-presenter-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:evr:video_presenter_' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed presenter function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
