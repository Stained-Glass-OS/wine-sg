#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Data.Xml.Dom gives back the node a call made (patches/sg/6545). windows.ui read the
# MSXML out-node of AppendChild (and InsertBefore, CloneNode, the parent/child/sibling getters,
# the prefix/namespace getters, SetNamedItem) in the same argument list as the call, before the
# call had filled it: AppendChild succeeded and gave no node. SnoreToast, KDE's toast helper
# (digiKam, Kdenlive), appends <audio> to a toast template and uses that node: it crashed.
# test/xmlappend-probe.c (Wine's headers: mingw has no Windows.Data.Xml.Dom C interfaces).
#   WINE=/opt/wine-sg/bin/wine test/xmlappend-gate.sh
# Mutant: SG_MUTANT_XML_OUT_BEFORE_CALL (dlls/windows.ui/xmldom.c: no node back, as before).
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
T=$(mktemp -d /var/tmp/sg-xmlappend.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
built=
W=$(dirname "$WINE")
for inc in "-I$W/include -I$W/../wine-10.0/include" "-I$W/../include/wine/windows" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O1 $inc -o "$T/probe.exe" "$HERE/xmlappend-probe.c" -lole32 -luuid -lruntimeobject 2>/dev/null &&
        { built=1; break; }
done
[ -n "$built" ] || { echo "SKIP: no Windows.Data.Xml.Dom headers (Wine's)"; exit 77; }
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check template            "a toast template (ToastText02)"
check append_returns_node "AppendChild gives back the appended node (SnoreToast's <audio>)"
check appended_usable     "and the node works (its attributes)"
check parent              "ParentNode gives a node"
check last_child          "LastChild gives a node"
check clone               "CloneNode gives a node"
check insert              "InsertBefore gives back the inserted node"
check local_name          "LocalName gives a value"
check done                "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
