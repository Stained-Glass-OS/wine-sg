#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI voice (patches/sg/2842), on Xvfb: test/sapi-voice-probe.c registers its
# own in-process text-to-speech engine and token so that Speak has an engine to
# drive, then checks ISpVoice's properties (priority, alert boundary, sync
# timeout), the event source (queue, interest masks, the Win32 event, window
# message, callback, sink and callback-interface notifications), GetStatus,
# SpeakCompleteEvent and WaitUntilDone, output token and stream, Skip through
# the engine site, SpeakStream (ANSI/UTF-16/UTF-8 text, wave streams) and
# SPF_IS_FILENAME, and the ISpeechVoice / ISpeechVoiceStatus automation
# late-bound through IDispatch (including AudioOutputStream = a file stream),
# the _ISpeechVoiceEvents connection point with an IDispatch sink (patch 2844)
# and Speak's SAPI XML (SPF_IS_XML: the fragments the engine gets, patch 2845).
# Before the patch these were FIXME stubs returning E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/sapi-voice-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (sapi, -DSG_MUTANT_x): SAPIVOICE_PRIORITY_ANY (any priority accepted),
# SAPIVOICE_EVENT_NO_QUEUE (events are not queued), SAPIVOICE_EVENT_STREAMNUM
# (events lose their stream number), SAPIVOICE_NOTIFY_TWICE (a second
# notification is accepted), SAPIVOICE_STATUS_STREAM (GetStatus stream numbers),
# SAPIVOICE_SKIP_ZERO (Skip always reports 0), SAPIVOICE_TEXT_ANSI (BOMs ignored),
# SAPIVOICE_OUTPUT_TOKEN (GetOutputObjectToken), SAPIVOICE_BOUNDARY_RANGE;
# connection points (patch 2844): SAPICP_UNADVISE_ANY, SAPICP_DIRECT (events are
# delivered on the engine thread), SAPICP_INTEREST, SAPICP_ADVISE_ANY,
# SAPICP_COOKIE_SAME; SAPI XML (patch 2845): SAPIXML_NO_DECODE, SAPIXML_NO_RESTORE
# (closing tags do not restore the state), SAPIXML_ACCEPT_BAD,
# SAPIXML_VOLUME_CLAMP, SAPIXML_PITCH_ABS, SAPIXML_SRC_OFFSET.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapivoice.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sapi-voice-probe.exe" "$HERE/sapi-voice-probe.c" -lsapi -luuid -lole32 -loleaut32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sapi-voice-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" sapi-voice-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
