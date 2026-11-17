#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An MSXML element's xml declares the namespaces it uses that are declared
# above it, as Windows' MSXML does, so that it stands on its own (wine-sg 7820).
# Office's Click-to-Run takes each product's <appv:Extensions> by its xml and
# loads that into a document of its own: without the xmlns:appv it found no
# appv:Extension there, the merged AppXManifest.xml had none of the products'
# extensions, and a direct OfficeSetup.exe install made no Word, Excel or
# PowerPoint shortcut in ProgramData's Start Menu\Programs. test/xmlscopens-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xmlscopens-gate.sh
# Mutant (dlls/msxml3/node.c): SG_MUTANT_XML_NO_SCOPE_NS -> no declarations, reloaded_extensions 0.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xmlscopens.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/xmlscopens-probe.exe" "$HERE/xmlscopens-probe.c" -lole32 -loleaut32 -luuid ||
    { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xmlscopens-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" xmlscopens-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx -- "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
X='http://schemas.microsoft.com/appx/2010/manifest'
V='http://schemas.microsoft.com/appv/2010/manifest'
chk 'loaded 1' "the manifest loads"
chk "extensions <appv:Extensions xmlns:appv=\"$V\"><appv:Extension Category=\"AppV.Shortcut\"><appv:Shortcut><appv:File>\[{Common Programs}\]\\\\Word.lnk</appv:File></appv:Shortcut></appv:Extension><appv:Extension Category=\"AppV.COM\"/></appv:Extensions>" \
    "a prefixed element declared above: its xml declares the prefix, once, on itself"
chk "properties <Properties xmlns:appv=\"$V\" xmlns=\"$X\"><appv:Desc>x</appv:Desc></Properties>" \
    "the default namespace too, and a prefix only its children use, on the element written"
chk "applications <Applications xmlns=\"$V\"><Application Id=\"Word\"/></Applications>" \
    "a namespace the element declares itself is written once"
chk "application <Application xmlns=\"$V\" Id=\"Word\"/>" "its child declares the default namespace it inherits"
chk "shortcutfile <appv:File xmlns:appv=\"$V\">\[{Common Programs}\]\\\\Word.lnk</appv:File>" "a leaf element"
chk 'empty <Empty xmlns=""/>' "an element in no namespace gets none added (only its own xmlns=\"\")"
chk 'doc_unchanged 1' "writing the xml leaves the document as it was"
chk 'reloaded 1' "the extensions' xml loads into a document of its own"
chk 'reloaded_extensions 2' "and its appv:Extension elements are found there (Click-to-Run's merge)"
chk 'reloaded_shortcuts 1' "with the shortcut's file"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
