#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What Telegram Desktop and Signal Desktop need of WinRT at start
# (patches/sg/0390, 0391).
#
# Signal: Windows.Foundation.Metadata.ApiInformation's IsPropertyPresent (and
# its other member queries) returned E_NOTIMPL, which C++/WinRT throws; the
# throw left a Node module's initialization and the main process stopped
# with "A JavaScript error occurred". Telegram: Windows.Storage.Streams
# .DataWriter could not be activated (no windows.storage.dll), and the
# exception ended the process before its window. The probe checks both
# answers, a DataWriter's bytes, and storing into and reading back an
# InMemoryRandomAccessStream.
#
#   WINE=/opt/wine-sg/bin/wine test/winrtstreams-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winrtstreams.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/winrtstreams-probe.exe" "$HERE/winrtstreams-probe.c" -lruntimeobject || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/winrtstreams-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() { printf '%s\n' "$out" | grep -q "^$1=1" && pass "$2" || fail "$2"; }
check api_property "ApiInformation.IsPropertyPresent answers (Signal)"
check api_method_event "ApiInformation.IsMethodPresent and IsEventPresent answer"
check api_type "ApiInformation.IsTypePresent: a registered class is present, an unknown one is not"
check datawriter_activate "a DataWriter activates (Telegram)"
check datawriter_bytes "DataWriter writes bytes, big and little endian numbers and UTF-8, and detaches them"
check memory_stream "an InMemoryRandomAccessStream activates"
check store_async "DataWriter.StoreAsync stores into the stream and calls its Completed handler"
check stream_read "the stream reads the bytes back (IBufferByteAccess)"
check stream_reference "RandomAccessStreamReference.CreateFromStream"
check done "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
