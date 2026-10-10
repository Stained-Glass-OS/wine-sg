#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# quartz Video Mixing Renderer stubs (patches/sg/2930), on Xvfb:
# test/quartz-vmr-probe.c drives the VMR9 and VMR7 filters (filter config,
# mixer control, mixer bitmap, windowless control, monitor config, COPP, the
# input pin's IAMVideoAccelerator and IOverlay), the VMR7 surface allocator
# notify object and the default allocator-presenter, checking every value.
# The run's log is also checked: none of those functions may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/quartz-vmr-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (quartz, -DSG_MUTANT_x): VMR9_STREAMS (SetNumberOfStreams takes any count),
# VMR9_ALPHA (SetAlpha accepts any value), VMR9_RECT (output rect not validated),
# VMR9_PROCAMP (ProcAmp reports success), VMR9_BITMAP (alpha bitmap not stored),
# VMR9_MONITOR (monitor index not validated), VMR9_PREFS (rendering prefs not validated),
# VMR7_MONITOR (monitor GUID not validated), VMR7_BORDER (border color not stored),
# OVERLAY_ADVISE (a second Advise succeeds), AMVA_GUIDS (reports an accelerator),
# VMR7_DDRAW (SetDDrawDevice keeps no reference), PRESENTER_POS (presenter keeps no position),
# COPP (COPP calls return E_NOTIMPL), ASPECT (aspect mode not validated), VMR7_PREFS (prefs not validated).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-quartzvmr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/quartz-vmr-probe.exe" "$HERE/quartz-vmr-probe.c" -lddraw -ldxguid -lstrmiids -luuid -lole32 -loleaut32 -luser32 -lgdi32 -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/quartz-vmr-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" quartz-vmr-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E 'fixme:quartz:(AMCertifiedOutputProtection_|certified_output_protection_|VMR9MonitorConfig_(Set|Get)(Default)?Monitor|monitor_config_(Set|Get)(Default)?Monitor|VMR9FilterConfig_|filter_config_|VMR9WindowlessControl_|windowless_control_|mixer_control9_|mixer_bitmap9_|video_accelerator_|overlay_|surface_allocator_notify_|image_presenter_(Start|Stop)Presenting|surface_allocator_AdviseNotify)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
