#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An installer's feature tree (patches/sg/0598). msi's tree subclass returned
# an HRESULT: item handles cut to 32 bits on 64-bit, so PuTTY's installer
# showed its features without their state icons and "FeaturesDlgItem-
# Description" / "FeaturesDlgItemSize" where the selected feature's
# description and size belong. A test package (msiseltree/: built with wixl,
# its dialog tables added with msibuild) is started; the tree's first item
# has its state icon, and the texts show the feature's description and size.
#
#   WINE=/opt/wine-sg/bin/wine test/msiseltree-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb wixl msibuild "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing (wixl, msitools)"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msiseltree.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/msiseltree-probe.exe" "$HERE/msiseltree-probe.c" -lcomctl32 || { echo "FAIL  probe did not build"; exit 1; }
cp "$HERE"/msiseltree/* "$T/" && cd "$T" && echo hi > a.txt && wixl -o tree.msi tree.wxs 2>/dev/null &&
    msibuild tree.msi -i Dialog.idt Control.idt EventMapping.idt ControlEvent.idt InstallUISequence.idt ||
    { echo "FAIL  the test package did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/msiseltree-probe.exe" "$T/tree.msi" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout 120 "$WINE" 'C:\msiseltree-probe.exe' 'C:\tree.msi' > "$T/out" 2>/dev/null </dev/null; r=$?
echo "      $(tr -d '\r' < "$T/out")"
[ $r = 0 ] && { echo "PASS  the feature tree has its state icons and shows the feature's description and size"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  the installer's feature tree"; echo "RESULT: FAIL"; exit 1
