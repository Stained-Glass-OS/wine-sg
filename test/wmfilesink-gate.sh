#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Windows Media file sink (patches/sg/9104; WMCreateWriterFileSink was a stub, fre:ac's WMA
# encoder died on it): a writer with a file sink of the program's own (IWMWriterAdvanced::AddSink),
# the file's final header from the sink, a reader gets the audio back; Close, auto-indexing
# (test/wmfilesink-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmfilesink-gate.sh
# Needs gstreamer1.0-libav (avenc_wmav2) and gstreamer1.0-plugins-ugly (asfdemux, for the reader).
# Mutants: SG_MUTANT_FILESINK_NO_FINISH (wmvcore filesink.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for so in libgstlibav.so libgstasf.so; do
    ls /usr/lib/*/gstreamer-1.0/$so >/dev/null 2>&1 || { echo "SKIP: no $so (gstreamer1.0-libav, -plugins-ugly)"; exit 77; }
done
T=$(mktemp -d /var/tmp/sg-wmfilesink.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmfilesink-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check export             "WMCreateWriterFileSink is exported"
check create             "WMCreateWriterFileSink makes a sink"
check interfaces         "IWMWriterSink, IWMWriterFileSink2 and 3"
check defaults           "auto-indexing on, single-buffer mode"
check not_realtime       "a file sink is not real time"
check open               "Open creates the file"
check add_sink           "AddSink takes it"
check begin              "BeginWriting with only the sink"
check write              "two seconds of PCM go in"
check end                "EndWriting"
check asf                "the file is an ASF file"
check final_size         "its file properties carry the file size"
check final_packets      "and the packet count"
check final_duration     "and the play duration"
check sink_size          "GetFileSize is the file size"
check sink_duration      "GetFileDuration is about two seconds"
check read_back          "a reader gives the two seconds back"
check auto_index_off     "SetAutoIndexing(FALSE) sticks"
check close              "Close closes it"
check second_sink        "a second sink on the same writer gets the whole file"
check closed_kept        "a closed sink keeps what it had and takes no more"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
