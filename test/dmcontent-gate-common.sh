#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Shared by the dmstyle / dmband / dmscript / dmcompos gates (patches 2960-2969): dmcontent-gate-common.sh is sourced by the
# gate scripts, which set GATE_NAME, PROBE, EXTRA_LIBS and FIXME_RE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-$GATE_NAME.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dmstyle,fixme+dmband,fixme+dmscript,fixme+dmcompos WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
# shellcheck disable=SC2086
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/$PROBE" $EXTRA_LIBS \
    || { echo "FAIL  probe did not build"; exit 1; }
# SG_GATE_PREFIX_CACHE=dir keeps the booted prefix between runs (the mutant runs repeat the gate many times)
if [ -n "${SG_GATE_PREFIX_CACHE:-}" ] && [ -f "$SG_GATE_PREFIX_CACHE/system.reg" ]; then
    cp -a "$SG_GATE_PREFIX_CACHE" "$WINEPREFIX"
else
    mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    [ -z "${SG_GATE_PREFIX_CACHE:-}" ] || { mkdir -p "$SG_GATE_PREFIX_CACHE" && cp -a "$WINEPREFIX/." "$SG_GATE_PREFIX_CACHE/"; }
fi
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" probe.exe ${PROBE_ARGS:-} 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E "fixme:dm(style|band|script|compos):($FIXME_RE)" "$T/stderr.log" | sed 's/^/FAIL  logged /' | grep .; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
