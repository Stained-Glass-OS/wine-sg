#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IWMMetadataEditor2::OpenEx (patches/sg/9103; the editor had no IWMMetadataEditor2, so fre:ac's WMA
# tagger read and wrote no tags): open for reading (Flush refused, file unchanged) and for writing
# with the given sharing, tags written and read back (test/wmeditor2-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmeditor2-gate.sh
# Needs gstreamer1.0-libav (avenc_wmav2) and gstreamer1.0-plugins-ugly (asfdemux, for the reader).
# Mutants: SG_MUTANT_WMED2_NO_HOLD (wmvcore editor.c)
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
T=$(mktemp -d /var/tmp/sg-wmeditor2.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmeditor2-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
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
check file               "a WMA file with a title"
check interface          "the editor has IWMMetadataEditor2"
check read_open          "OpenEx for reading"
check read_tags          "the attributes, as fre:ac walks them"
check read_shared        "FILE_SHARE_READ lets another reader in"
check read_no_flush      "Flush fails on a file opened for reading, the file unchanged"
check read_close         "Close"
check write_open         "OpenEx for writing, no sharing"
check write_exclusive    "no sharing keeps another opener out"
check add                "new attributes"
check write_flush        "Flush writes them"
check still_exclusive    "the editor still holds the file after Flush"
check released           "Close lets it go"
check written            "the new tags read back"
check title_kept         "the title is kept"
check missing            "a missing file fails"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
